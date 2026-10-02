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

  void setMeshData(const AvatarMeshData &meshData);

  void start();
  void stop();

  void playClip(const QString &name);

  void setSurface(AvatarSurface *surface);

  bool isLoaded() const { return m_skeletonLoaded; }

  void applyViseme(const QString &viseme);
  void applyVisemeBlend(const QString &from, const QString &to, float t);

  void setBlinkEnabled(bool enabled);
  bool isBlinkEnabled() const { return m_blinkEnabled; }

  void setVisemeGain(float gain);
  float visemeGain() const { return m_visemeGain; }

  void setSpeaking(bool speaking);
  bool isSpeaking() const { return m_speaking; }

signals:
  void ticked();

private slots:
  void onTick();

private:
  void updateJoints(float dt);

  bool buildSkinBindings();
  void buildMorphData();

  void updateBlink(int dtMs);
  void updateGaze(int dtMs);
  void updateExpression(int dtMs);

  void pushFaceWeights();

  void scheduleNextBlink();
  void scheduleNextGaze();

  enum class BlinkState {
    Idle,
    Closing,
    Opening,
  };

  enum class GazeState {
    Holding,
    Moving,
  };

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
  bool m_meshDataValid = false;

  QVector<int> m_skinToOzzJoint;
  QVector<QMatrix4x4> m_inverseBindPoses;

  std::vector<QMatrix4x4> m_skinningMatrices;

  std::vector<float> m_visemeWeights;
  std::vector<float> m_expressionWeights;
  std::vector<float> m_gazeWeights;
  std::vector<float> m_blinkWeights;
  std::vector<float> m_morphWeights;

  int m_blinkLeftIndex = -1;
  int m_blinkRightIndex = -1;

  int m_browInnerLeft = -1;
  int m_browInnerRight = -1;
  int m_browOuterLeft = -1;
  int m_browOuterRight = -1;
  int m_cheekRaiseLeft = -1;
  int m_cheekRaiseRight = -1;
  int m_eyeSquintLeft = -1;
  int m_eyeSquintRight = -1;
  int m_mouthSmileLeft = -1;
  int m_mouthSmileRight = -1;

  int m_gazeLeftL = -1;
  int m_gazeLeftR = -1;
  int m_gazeRightL = -1;
  int m_gazeRightR = -1;
  int m_gazeUpL = -1;
  int m_gazeUpR = -1;
  int m_gazeDownL = -1;
  int m_gazeDownR = -1;

  bool m_blinkEnabled = true;
  BlinkState m_blinkState = BlinkState::Idle;
  int m_blinkElapsedMs = 0;
  int m_blinkNextMs = 0;

  int m_blinkCloseDurationMs = 60;
  int m_blinkOpenDurationMs = 90;
  int m_blinkMinIntervalMs = 2000;
  int m_blinkMaxIntervalMs = 6000;

  GazeState m_gazeState = GazeState::Holding;
  int m_gazeElapsedMs = 0;
  int m_gazeHoldMs = 1500;

  float m_gazeTargetX = 0.0f;
  float m_gazeTargetY = 0.0f;
  float m_gazeCurrentX = 0.0f;
  float m_gazeCurrentY = 0.0f;

  int m_gazeMoveDurationMs = 90;
  int m_gazeMinHoldMs = 600;
  int m_gazeMaxHoldMs = 1800;

  float m_browTarget = 0.0f;
  float m_browCurrent = 0.0f;
  float m_cheekTarget = 0.0f;
  float m_cheekCurrent = 0.0f;
  float m_squintTarget = 0.0f;
  float m_squintCurrent = 0.0f;
  float m_smileTarget = 0.0f;
  float m_smileCurrent = 0.0f;

  QString m_lastViseme;

  bool m_speaking = false;

  float m_visemeGain = 1.35f;

  VisemeTable m_visemeTable;

  AvatarSurface *m_surface = nullptr;

  QTimer *m_timer = nullptr;

  QStringList m_jointNames;
};