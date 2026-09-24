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

#include "AvatarMeshData.h"
#include "VisemeTable.h"
#include "voice/VisemeMap.h"

class AvatarSurface;
class QQuickItem;

class AvatarController : public QObject {
  Q_OBJECT

public:
  explicit AvatarController(QObject *parent = nullptr);
  ~AvatarController() override;

  bool loadArchives(const QString &skeletonPath, const QString &clipDir);

  bool attachToScene(QQuickItem *qmlRoot, const QString &nodeName);

  void start();
  void stop();

  void playClip(const QString &name);

  void setSurface(AvatarSurface *surface);

  bool isLoaded() const { return m_skeletonLoaded; }

  // Apply a viseme by name. The face mesh's morph weights are set to
  // the mapped shape weights and pushed to the surface on the next
  // frame. "sil" and unknown names clear all weights.
  void applyViseme(const QString &viseme);

signals:
  void ticked();

private slots:
  void onTick();

private:
  void updateJoints(float dt);

  bool buildSkinBindings();
  void buildMorphData();

  ozz::animation::Skeleton m_skeleton;
  bool m_skeletonLoaded = false;

  QHash<QString, std::shared_ptr<ozz::animation::Animation>> m_clips;
  QString m_currentClip;

  std::unique_ptr<ozz::animation::SamplingJob::Context> m_samplingContext;

  std::vector<ozz::math::SoaTransform> m_localTransforms;
  std::vector<ozz::math::Float4x4> m_modelSpaceMatrices;

  float m_time = 0.0f;
  float m_clipDuration = 0.0f;

  AvatarMeshData m_meshData;

  QVector<int> m_skinToOzzJoint;
  QVector<QMatrix4x4> m_inverseBindPoses;

  std::vector<QMatrix4x4> m_skinningMatrices;

  // Per-target weights for the face mesh, one float per morph target
  // in the face mesh's target order. Zero when silent.
  std::vector<float> m_morphWeights;

  VisemeTable m_visemeTable;

  AvatarSurface *m_surface = nullptr;

  QTimer *m_timer = nullptr;

  QStringList m_jointNames;
};