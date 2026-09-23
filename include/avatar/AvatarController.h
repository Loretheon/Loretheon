#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QHash>
#include <QVector>
#include <QTimer>

#include <memory>
#include <vector>

#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/animation/runtime/sampling_job.h>
#include <ozz/animation/runtime/local_to_model_job.h>
#include <ozz/base/maths/soa_transform.h>
#include <ozz/base/maths/vec_float.h>
#include <ozz/base/maths/simd_math.h>
#include <ozz/base/io/archive.h>
#include <ozz/base/io/stream.h>

class QQuickItem;

// Owns the ozz runtime, the loaded skeleton and clips, and the bridge
// into the Qt Quick 3D scene graph.
//
// The controller is created from C++ (by AvatarWidget) and lives for
// the lifetime of the avatar. It loads the ozz archives at startup,
// keeps one Animation object per clip in memory, and on each tick
// samples the current clip into local-space joint transforms, converts
// those to model-space matrices, and writes them into the Qt Quick 3D
// joint nodes.
//
// It does not render. Qt Quick 3D renders. The controller's job ends
// when the joint Node transforms have been updated.
class AvatarController : public QObject {
  Q_OBJECT

public:
  explicit AvatarController(QObject *parent = nullptr);
  ~AvatarController() override;

  // Load the ozz archives. skeletonPath is the skeleton.ozz produced
  // by gltf2ozz. clipDir is the directory containing the per-clip .ozz
  // files. Returns false if the skeleton cannot be loaded; clip load
  // failures are logged and skipped.
  bool loadArchives(const QString &skeletonPath, const QString &clipDir);

  // Attach the controller to the QML scene. Call this after the QML
  // root object is created. The controller walks the scene graph to
  // find the Node that the model was instantiated into, and binds its
  // joint Node children by name to ozz joint indices.
  //
  // qmlRoot is the QQuickItem returned by AvatarWidget::rootObject().
  // nodeName is the objectName of the Node that the GLB was loaded
  // into.
  bool attachToScene(QQuickItem *qmlRoot, const QString &nodeName);

  // Start and stop the tick timer. The timer runs at 60 Hz.
  void start();
  void stop();

  // Play the named clip. Crossfading is not implemented yet; this
  // replaces the current clip. Names are the file basenames of the
  // .ozz files, e.g. "walk", "idle", "jump".
  void playClip(const QString &name);

  bool isLoaded() const { return m_skeletonLoaded; }

signals:
  // Emitted once per tick after joints have been updated. Useful for
  // debugging and for anything that wants to observe the clock.
  void ticked();

private slots:
  void onTick();

private:
  void updateJoints(float dt);

  // --- ozz runtime state ---

  ozz::animation::Skeleton m_skeleton;
  bool m_skeletonLoaded = false;

  QHash<QString, std::shared_ptr<ozz::animation::Animation>> m_clips;
  QString m_currentClip;

  // Per-clip sampling context. Reallocated on clip change because the
  // context caches the previous sample for interpolation.
  std::unique_ptr<ozz::animation::SamplingJob::Context> m_samplingContext;

  // Scratch buffers, sized to the skeleton joint count. Reused every
  // tick so no allocation happens in the hot path.
  std::vector<ozz::math::SoaTransform> m_localTransforms;
  std::vector<ozz::math::Float4x4> m_modelSpaceMatrices;

  float m_time = 0.0f;
  float m_clipDuration = 0.0f;

  // --- scene graph binding ---

  // One entry per skeleton joint, in ozz order. Each entry is the
  // QML Node whose objectName matched the joint name, or nullptr if
  // no match was found. Held as QObject* because QQuick3DNode is not
  // a public type; all access goes through QObject::setProperty.
  QVector<QObject *> m_jointNodes;
  bool m_attached = false;

  QTimer *m_timer = nullptr;

  // The list of joint names in ozz order. Used to look up the
  // corresponding QML nodes by objectName.
  QStringList m_jointNames;
};