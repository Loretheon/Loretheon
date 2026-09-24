#include "../../include/avatar/AvatarController.h"

#include "../../include/avatar/AvatarSurface.h"

#include "../../include/avatar/AvatarMeshLoader.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMatrix4x4>
#include <QQuickItem>
#include <QRandomGenerator>
#include <QTimer>
#include <QVariant>

#include <cmath>

#include <ozz/animation/runtime/animation.h>
#include <ozz/animation/runtime/local_to_model_job.h>
#include <ozz/animation/runtime/sampling_job.h>
#include <ozz/animation/runtime/skeleton.h>
#include <ozz/base/io/archive.h>
#include <ozz/base/io/stream.h>
#include <ozz/base/maths/simd_math.h>
#include <ozz/base/maths/soa_transform.h>
#include <ozz/base/maths/vec_float.h>

namespace {

QMatrix4x4 toQMatrix4x4(const ozz::math::Float4x4 &m) {
  QMatrix4x4 result;

  const ozz::math::SimdFloat4 &c0 = m.cols[0];
  const ozz::math::SimdFloat4 &c1 = m.cols[1];
  const ozz::math::SimdFloat4 &c2 = m.cols[2];
  const ozz::math::SimdFloat4 &c3 = m.cols[3];

  result.setColumn(0, QVector4D(ozz::math::GetX(c0), ozz::math::GetY(c0),
                                ozz::math::GetZ(c0), ozz::math::GetW(c0)));

  result.setColumn(1, QVector4D(ozz::math::GetX(c1), ozz::math::GetY(c1),
                                ozz::math::GetZ(c1), ozz::math::GetW(c1)));

  result.setColumn(2, QVector4D(ozz::math::GetX(c2), ozz::math::GetY(c2),
                                ozz::math::GetZ(c2), ozz::math::GetW(c2)));

  result.setColumn(3, QVector4D(ozz::math::GetX(c3), ozz::math::GetY(c3),
                                ozz::math::GetZ(c3), ozz::math::GetW(c3)));

  return result;
}

QByteArray readResource(const QString &path) {
  QFile file(path);

  if (!file.open(QIODevice::ReadOnly)) {
    qWarning() << "[AvatarController] Cannot open resource:" << path
               << file.errorString();
    return {};
  }

  const QByteArray data = file.readAll();
  file.close();

  return data;
}

float approach(float current, float target, float rate, int dtMs) {
  const float t = 1.0f - std::exp(-rate * dtMs / 1000.0f);
  return current + (target - current) * t;
}

} // namespace

AvatarController::AvatarController(QObject *parent) : QObject(parent) {
  m_timer = new QTimer(this);
  m_timer->setInterval(16);
  connect(m_timer, &QTimer::timeout, this, &AvatarController::onTick);

  m_blinkNextMs = QRandomGenerator::global()->bounded(
      m_blinkMinIntervalMs, m_blinkMaxIntervalMs + 1);

  m_gazeHoldMs = QRandomGenerator::global()->bounded(
      m_gazeMinHoldMs, m_gazeMaxHoldMs + 1);
}

AvatarController::~AvatarController() = default;

void AvatarController::setSurface(AvatarSurface *surface) {
  m_surface = surface;
}

void AvatarController::setBlinkEnabled(bool enabled) {
  m_blinkEnabled = enabled;

  if (!enabled) {
    std::fill(m_blinkWeights.begin(), m_blinkWeights.end(), 0.0f);
    m_blinkState = BlinkState::Idle;
    m_blinkElapsedMs = 0;
    scheduleNextBlink();
    pushFaceWeights();
  }
}

void AvatarController::setVisemeGain(float gain) {
  m_visemeGain = qBound(0.0f, gain, 3.0f);
  pushFaceWeights();
}

void AvatarController::setSpeaking(bool speaking) {
  m_speaking = speaking;
}

bool AvatarController::loadArchives(const QString &skeletonPath,
                                    const QString &clipDir) {
  auto normalizeResourcePath = [](const QString &path) -> QString {
    if (path.startsWith(QStringLiteral("qrc:/"))) {
      return QStringLiteral(":") + path.mid(4);
    }
    return path;
  };

  const QString dirPath = normalizeResourcePath(clipDir);
  const QString skelPath = normalizeResourcePath(skeletonPath);

  {
    QDir dir(dirPath);

    if (!dir.exists()) {
      qWarning() << "[AvatarController] Resource directory does not exist:"
                 << dirPath;
      return false;
    }
  }

  {
    const QByteArray bytes = readResource(skelPath);

    if (bytes.isEmpty()) {
      qWarning() << "[AvatarController] Skeleton resource is empty or"
                 << "unreadable:" << skelPath;
      return false;
    }

    ozz::io::MemoryStream stream;
    stream.Write(bytes.constData(), static_cast<size_t>(bytes.size()));
    stream.Seek(0, ozz::io::Stream::kSet);

    ozz::io::IArchive archive(&stream);

    if (!archive.TestTag<ozz::animation::Skeleton>()) {
      qWarning() << "[AvatarController] Resource is not an ozz skeleton"
                 << "archive:" << skelPath;
      return false;
    }

    archive >> m_skeleton;

    if (!m_skeleton.num_joints()) {
      qWarning() << "[AvatarController] Skeleton deserialized with zero"
                 << "joints.";
      return false;
    }
  }

  m_skeletonLoaded = true;

  qDebug() << "[AvatarController] Loaded skeleton with"
           << m_skeleton.num_joints() << "joints.";

  m_jointNames.clear();
  m_jointNames.reserve(static_cast<int>(m_skeleton.num_joints()));

  for (int i = 0; i < m_skeleton.num_joints(); ++i) {
    m_jointNames.append(QString::fromUtf8(m_skeleton.joint_names()[i]));
  }

  m_localTransforms.resize(m_skeleton.num_joints());
  m_modelSpaceMatrices.resize(m_skeleton.num_joints());

  qDebug() << "[AvatarController] Scratch buffers sized to"
           << m_localTransforms.size() << "transforms and"
           << m_modelSpaceMatrices.size() << "matrices.";

  QDir dir(dirPath);

  const QStringList clipFiles =
      dir.entryList(QStringList{QStringLiteral("*.ozz")},
                    QDir::Files | QDir::Readable, QDir::Name);

  for (const QString &file : clipFiles) {
    if (file == QStringLiteral("skeleton.ozz")) {
      continue;
    }

    const QString fullPath = dir.filePath(file);
    const QByteArray bytes = readResource(fullPath);

    if (bytes.isEmpty()) {
      qWarning() << "[AvatarController] Skipping clip, resource empty or"
                 << "unreadable:" << fullPath;
      continue;
    }

    ozz::io::MemoryStream stream;
    stream.Write(bytes.constData(), static_cast<size_t>(bytes.size()));
    stream.Seek(0, ozz::io::Stream::kSet);

    ozz::io::IArchive archive(&stream);

    if (!archive.TestTag<ozz::animation::Animation>()) {
      qWarning() << "[AvatarController] Skipping clip, wrong archive tag:"
                 << fullPath;
      continue;
    }

    auto animation = std::make_shared<ozz::animation::Animation>();
    archive >> *animation;

    const QString name = QFileInfo(file).completeBaseName();

    m_clips.insert(name, animation);

    qDebug() << "[AvatarController] Loaded clip:" << name
             << "duration:" << animation->duration() << "s";
  }

  if (m_clips.isEmpty()) {
    qWarning() << "[AvatarController] No clips loaded.";
    return false;
  }

  qDebug() << "[AvatarController] Archives loaded:"
           << m_clips.size() << "clips.";

  return true;
}

bool AvatarController::attachToScene(QQuickItem *qmlRoot,
                                     const QString &nodeName) {
  Q_UNUSED(qmlRoot);
  Q_UNUSED(nodeName);

  return true;
}

bool AvatarController::buildSkinBindings() {
  if (m_meshData.skins.isEmpty()) {
    qWarning() << "[AvatarController] No skin in the mesh data. The"
               << "character cannot be skinned.";
    return false;
  }

  const AvatarSkin &skin = m_meshData.skins.first();

  m_skinToOzzJoint.clear();
  m_skinToOzzJoint.reserve(skin.jointCount());

  int matched = 0;

  for (const QString &skinJointName : skin.jointNames) {
    int ozzIndex = -1;

    for (int i = 0; i < m_jointNames.size(); ++i) {
      if (m_jointNames.at(i) == skinJointName) {
        ozzIndex = i;
        break;
      }
    }

    if (ozzIndex >= 0) {
      ++matched;
    }

    m_skinToOzzJoint.append(ozzIndex);
  }

  qDebug() << "[AvatarController] Skin binding:"
           << matched << "of" << skin.jointCount()
           << "skin joints matched to ozz joints.";

  m_inverseBindPoses.clear();
  m_inverseBindPoses.reserve(skin.jointCount());

  for (int j = 0; j < skin.jointCount(); ++j) {
    const float *f = skin.inverseBindPoses.constData() + j * 16;

    QMatrix4x4 m;
    m.setColumn(0, QVector4D(f[0], f[1], f[2], f[3]));
    m.setColumn(1, QVector4D(f[4], f[5], f[6], f[7]));
    m.setColumn(2, QVector4D(f[8], f[9], f[10], f[11]));
    m.setColumn(3, QVector4D(f[12], f[13], f[14], f[15]));

    m_inverseBindPoses.append(m);
  }

  m_skinningMatrices.resize(static_cast<size_t>(skin.jointCount()));

  for (auto &m : m_skinningMatrices) {
    m.setToIdentity();
  }

  return true;
}

void AvatarController::buildMorphData() {
  m_meshData.faceMorphNameToIndex.clear();

  if (m_meshData.faceMeshIndex < 0 ||
      m_meshData.faceMeshIndex >= m_meshData.meshes.size()) {
    qWarning() << "[AvatarController] No face mesh; morph targets will"
               << "not be driven.";
    return;
  }

  const AvatarMesh &face = m_meshData.meshes.at(m_meshData.faceMeshIndex);

  if (face.primitives.isEmpty()) {
    return;
  }

  for (const AvatarPrimitive &prim : face.primitives) {
    for (int t = 0; t < prim.morphTargetNames.size(); ++t) {
      const QString &name = prim.morphTargetNames.at(t);

      if (name.isEmpty()) {
        continue;
      }

      if (!m_meshData.faceMorphNameToIndex.contains(name)) {
        m_meshData.faceMorphNameToIndex.insert(name, t);
      }
    }
  }

  m_visemeTable.build(m_meshData.faceMorphNameToIndex);

  int targetCount = 0;

  for (const AvatarPrimitive &prim : face.primitives) {
    targetCount = qMax(targetCount, prim.morphTargetCount());
  }

  m_visemeWeights.assign(static_cast<size_t>(targetCount), 0.0f);
  m_expressionWeights.assign(static_cast<size_t>(targetCount), 0.0f);
  m_gazeWeights.assign(static_cast<size_t>(targetCount), 0.0f);
  m_blinkWeights.assign(static_cast<size_t>(targetCount), 0.0f);
  m_morphWeights.assign(static_cast<size_t>(targetCount), 0.0f);

  auto indexOf = [this](const char *name) {
    return m_meshData.faceMorphNameToIndex.value(
        QString::fromLatin1(name), -1);
  };

  m_blinkLeftIndex = indexOf("Eye_Blink_L");
  m_blinkRightIndex = indexOf("Eye_Blink_R");

  m_browInnerLeft = indexOf("Brow_Raise_Inner_L");
  m_browInnerRight = indexOf("Brow_Raise_Inner_R");
  m_browOuterLeft = indexOf("Brow_Raise_Outer_L");
  m_browOuterRight = indexOf("Brow_Raise_Outer_R");
  m_cheekRaiseLeft = indexOf("Cheek_Raise_L");
  m_cheekRaiseRight = indexOf("Cheek_Raise_R");
  m_eyeSquintLeft = indexOf("Eye_Squint_L");
  m_eyeSquintRight = indexOf("Eye_Squint_R");
  m_mouthSmileLeft = indexOf("Mouth_Smile_L");
  m_mouthSmileRight = indexOf("Mouth_Smile_R");

  m_gazeLeftL = indexOf("Eye_L_Look_L");
  m_gazeLeftR = indexOf("Eye_R_Look_L");
  m_gazeRightL = indexOf("Eye_L_Look_R");
  m_gazeRightR = indexOf("Eye_R_Look_R");
  m_gazeUpL = indexOf("Eye_L_Look_Up");
  m_gazeUpR = indexOf("Eye_R_Look_Up");
  m_gazeDownL = indexOf("Eye_L_Look_Down");
  m_gazeDownR = indexOf("Eye_R_Look_Down");

  qDebug() << "[AvatarController] Morph data:"
           << m_meshData.faceMorphNameToIndex.size() << "named targets,"
           << targetCount << "weight slots,"
           << "blink L/R" << m_blinkLeftIndex << m_blinkRightIndex
           << "brow" << m_browInnerLeft << m_browOuterLeft
           << "cheek" << m_cheekRaiseLeft
           << "squint" << m_eyeSquintLeft
           << "smile" << m_mouthSmileLeft
           << "gaze L/R" << m_gazeLeftL << m_gazeRightL;
}

void AvatarController::start() {
  QString error;

  if (!AvatarMeshLoader::load(QStringLiteral(":/avatar/ccbase/Lore.glb"),
                              m_meshData, &error)) {
    qWarning() << "[AvatarController] Mesh load failed:" << error;
    return;
  }

  if (!buildSkinBindings()) {
    qWarning() << "[AvatarController] Skin binding build failed. The"
               << "character will draw in bind pose.";
  }

  buildMorphData();

  if (m_surface) {
    m_surface->setSkinningMatrices(m_skinningMatrices);
    m_surface->setMorphWeights(m_morphWeights);
  }

  m_time = 0.0f;
  m_timer->start();
}

void AvatarController::stop() { m_timer->stop(); }

void AvatarController::playClip(const QString &name) {
  if (!m_clips.contains(name)) {
    qWarning() << "[AvatarController] No clip named" << name;
    return;
  }

  if (m_currentClip == name) {
    return;
  }

  m_currentClip = name;
  m_time = 0.0f;
  m_clipDuration = m_clips.value(name)->duration();

  m_samplingContext =
      std::make_unique<ozz::animation::SamplingJob::Context>();
  m_samplingContext->Resize(m_skeleton.num_joints());

  qDebug() << "[AvatarController] Playing clip:" << name
           << "duration:" << m_clipDuration << "s";
}

void AvatarController::applyViseme(const QString &viseme) {
  if (m_visemeWeights.empty()) {
    return;
  }

  m_lastViseme = viseme;

  std::fill(m_visemeWeights.begin(), m_visemeWeights.end(), 0.0f);

  if (viseme != QStringLiteral("sil") && !viseme.isEmpty()) {
    const QVector<VisemeTable::Weight> weights =
        m_visemeTable.weightsFor(viseme);

    for (const VisemeTable::Weight &w : weights) {
      if (w.morphIndex < 0 ||
          w.morphIndex >= static_cast<int>(m_visemeWeights.size())) {
        continue;
      }

      m_visemeWeights[static_cast<size_t>(w.morphIndex)] = w.weight;
    }

    static int counter = 0;

    if (++counter % 60 == 0 &&
        (viseme == QStringLiteral("aa") ||
         viseme == QStringLiteral("E") ||
         viseme == QStringLiteral("O") ||
         viseme == QStringLiteral("U"))) {
      float jaw = 0.0f;
      float vOpen = 0.0f;
      float mouthUp = 0.0f;
      float dropLower = 0.0f;

      if (m_meshData.faceMorphNameToIndex.contains("Jaw_Open")) {
        const int idx = m_meshData.faceMorphNameToIndex.value("Jaw_Open");
        if (idx >= 0 && idx < static_cast<int>(m_visemeWeights.size())) {
          jaw = m_visemeWeights[static_cast<size_t>(idx)];
        }
      }

      if (m_meshData.faceMorphNameToIndex.contains("V_Open")) {
        const int idx = m_meshData.faceMorphNameToIndex.value("V_Open");
        if (idx >= 0 && idx < static_cast<int>(m_visemeWeights.size())) {
          vOpen = m_visemeWeights[static_cast<size_t>(idx)];
        }
      }

      if (m_meshData.faceMorphNameToIndex.contains("Mouth_Up")) {
        const int idx = m_meshData.faceMorphNameToIndex.value("Mouth_Up");
        if (idx >= 0 && idx < static_cast<int>(m_visemeWeights.size())) {
          mouthUp = m_visemeWeights[static_cast<size_t>(idx)];
        }
      }

      if (m_meshData.faceMorphNameToIndex.contains("Mouth_Drop_Lower")) {
        const int idx =
            m_meshData.faceMorphNameToIndex.value("Mouth_Drop_Lower");
        if (idx >= 0 && idx < static_cast<int>(m_visemeWeights.size())) {
          dropLower = m_visemeWeights[static_cast<size_t>(idx)];
        }
      }

      qDebug() << "[AvatarController] viseme" << viseme
               << "jaw" << jaw
               << "V_Open" << vOpen
               << "Mouth_Up" << mouthUp
               << "Drop_Lower" << dropLower
               << "gain" << m_visemeGain;
    }
  }

  pushFaceWeights();
}

void AvatarController::applyVisemeBlend(const QString &from,
                                        const QString &to,
                                        float t) {
  if (m_visemeWeights.empty()) {
    return;
  }

  t = qBound(0.0f, t, 1.0f);

  m_lastViseme = (t < 0.5f) ? from : to;

  std::fill(m_visemeWeights.begin(), m_visemeWeights.end(), 0.0f);

  const QVector<VisemeTable::Weight> fromWeights =
      m_visemeTable.weightsFor(from);
  const QVector<VisemeTable::Weight> toWeights =
      m_visemeTable.weightsFor(to);

  const float invT = 1.0f - t;

  for (const VisemeTable::Weight &w : fromWeights) {
    if (w.morphIndex < 0 ||
        w.morphIndex >= static_cast<int>(m_visemeWeights.size())) {
      continue;
    }

    m_visemeWeights[static_cast<size_t>(w.morphIndex)] += w.weight * invT;
  }

  for (const VisemeTable::Weight &w : toWeights) {
    if (w.morphIndex < 0 ||
        w.morphIndex >= static_cast<int>(m_visemeWeights.size())) {
      continue;
    }

    m_visemeWeights[static_cast<size_t>(w.morphIndex)] += w.weight * t;
  }

  pushFaceWeights();
}

void AvatarController::scheduleNextBlink() {
  m_blinkNextMs = QRandomGenerator::global()->bounded(
      m_blinkMinIntervalMs, m_blinkMaxIntervalMs + 1);
}

void AvatarController::updateBlink(int dtMs) {
  if (!m_blinkEnabled) {
    return;
  }

  if (m_blinkLeftIndex < 0 || m_blinkRightIndex < 0) {
    return;
  }

  bool changed = false;

  if (m_blinkState == BlinkState::Idle) {
    m_blinkElapsedMs += dtMs;

    if (m_blinkElapsedMs >= m_blinkNextMs) {
      m_blinkState = BlinkState::Closing;
      m_blinkElapsedMs = 0;
    }
  } else if (m_blinkState == BlinkState::Closing) {
    m_blinkElapsedMs += dtMs;

    float t = static_cast<float>(m_blinkElapsedMs) /
              static_cast<float>(m_blinkCloseDurationMs);

    if (t >= 1.0f) {
      t = 1.0f;
      m_blinkState = BlinkState::Opening;
      m_blinkElapsedMs = 0;
    }

    m_blinkWeights[static_cast<size_t>(m_blinkLeftIndex)] = t;
    m_blinkWeights[static_cast<size_t>(m_blinkRightIndex)] = t;
    changed = true;
  } else if (m_blinkState == BlinkState::Opening) {
    m_blinkElapsedMs += dtMs;

    float t = 1.0f - static_cast<float>(m_blinkElapsedMs) /
                         static_cast<float>(m_blinkOpenDurationMs);

    if (t <= 0.0f) {
      t = 0.0f;
      m_blinkState = BlinkState::Idle;
      m_blinkElapsedMs = 0;
      scheduleNextBlink();
    }

    m_blinkWeights[static_cast<size_t>(m_blinkLeftIndex)] = t;
    m_blinkWeights[static_cast<size_t>(m_blinkRightIndex)] = t;
    changed = true;
  }

  if (changed) {
    pushFaceWeights();
  }
}

void AvatarController::scheduleNextGaze() {
  m_gazeHoldMs = QRandomGenerator::global()->bounded(
      m_gazeMinHoldMs, m_gazeMaxHoldMs + 1);
}

void AvatarController::updateGaze(int dtMs) {
  if (m_gazeLeftL < 0 || m_gazeRightL < 0) {
    return;
  }

  const int moveDurationMs = m_speaking ? 70 : 90;
  const int minHoldMs = m_speaking ? 350 : 600;
  const int maxHoldMs = m_speaking ? 1100 : 1800;
  const float stepX = m_speaking ? 1.3f : 0.9f;
  const float stepY = m_speaking ? 0.7f : 0.5f;
  const float clampX = m_speaking ? 0.45f : 0.60f;
  const float clampY = m_speaking ? 0.25f : 0.35f;

  bool changed = false;

  if (m_gazeState == GazeState::Holding) {
    m_gazeElapsedMs += dtMs;

    if (m_gazeElapsedMs >= m_gazeHoldMs) {
      auto rg = QRandomGenerator::global();

      const float dx = (rg->generateDouble() - 0.5f) * stepX;
      const float dy = (rg->generateDouble() - 0.5f) * stepY;

      m_gazeTargetX = qBound(-clampX, m_gazeTargetX + dx, clampX);
      m_gazeTargetY = qBound(-clampY, m_gazeTargetY + dy, clampY);

      m_gazeState = GazeState::Moving;
      m_gazeElapsedMs = 0;
    }
  } else if (m_gazeState == GazeState::Moving) {
    m_gazeElapsedMs += dtMs;

    const float t = static_cast<float>(m_gazeElapsedMs) /
                    static_cast<float>(moveDurationMs);

    if (t >= 1.0f) {
      m_gazeCurrentX = m_gazeTargetX;
      m_gazeCurrentY = m_gazeTargetY;
      m_gazeState = GazeState::Holding;
      m_gazeElapsedMs = 0;
      m_gazeHoldMs = QRandomGenerator::global()->bounded(
          minHoldMs, maxHoldMs + 1);
    } else {
      m_gazeCurrentX = approach(m_gazeCurrentX, m_gazeTargetX,
                                20.0f, dtMs);
      m_gazeCurrentY = approach(m_gazeCurrentY, m_gazeTargetY,
                                20.0f, dtMs);
    }

    changed = true;
  }

  if (!changed) {
    return;
  }

  const float gx = m_gazeCurrentX;
  const float gy = m_gazeCurrentY;

  const float lookRight = gx > 0.0f ? gx : 0.0f;
  const float lookLeft = gx < 0.0f ? -gx : 0.0f;
  const float lookUp = gy > 0.0f ? gy : 0.0f;
  const float lookDown = gy < 0.0f ? -gy : 0.0f;

  auto set = [this](int index, float value) {
    if (index >= 0 &&
        index < static_cast<int>(m_gazeWeights.size())) {
      m_gazeWeights[static_cast<size_t>(index)] = value;
    }
  };

  set(m_gazeLeftL, lookLeft);
  set(m_gazeLeftR, lookLeft);
  set(m_gazeRightL, lookRight);
  set(m_gazeRightR, lookRight);
  set(m_gazeUpL, lookUp);
  set(m_gazeUpR, lookUp);
  set(m_gazeDownL, lookDown);
  set(m_gazeDownR, lookDown);

  pushFaceWeights();
}

void AvatarController::updateExpression(int dtMs) {
  constexpr float kRestingSquint = 0.15f;
  constexpr float kBaseSmile = 0.30f;

  float browTarget = 0.12f;
  float cheekTarget = 0.18f;
  float squintTarget = kRestingSquint;
  float smileTarget = kBaseSmile;

  if (m_speaking) {
    browTarget = 0.28f;
    cheekTarget = 0.35f;
    smileTarget = kBaseSmile + 0.25f;

    if (m_lastViseme == QStringLiteral("aa") ||
        m_lastViseme == QStringLiteral("O") ||
        m_lastViseme == QStringLiteral("E")) {
      browTarget += 0.22f;
      cheekTarget += 0.30f;
    }

    if (m_lastViseme == QStringLiteral("I") ||
        m_lastViseme == QStringLiteral("E")) {
      cheekTarget += 0.28f;
      smileTarget += 0.20f;
    }

    if (m_lastViseme == QStringLiteral("O") ||
        m_lastViseme == QStringLiteral("U")) {
      cheekTarget += 0.18f;
    }

    if (m_lastViseme == QStringLiteral("SS") ||
        m_lastViseme == QStringLiteral("nn") ||
        m_lastViseme == QStringLiteral("PP") ||
        m_lastViseme == QStringLiteral("FF") ||
        m_lastViseme == QStringLiteral("TH")) {
      squintTarget += 0.12f;
      browTarget -= 0.04f;
    }

    if (m_lastViseme == QStringLiteral("RR")) {
      browTarget += 0.15f;
    }

    if (m_lastViseme == QStringLiteral("DD") ||
        m_lastViseme == QStringLiteral("kk") ||
        m_lastViseme == QStringLiteral("CH")) {
      browTarget += 0.08f;
    }
  }

  browTarget = qBound(0.0f, browTarget, 0.70f);
  cheekTarget = qBound(0.0f, cheekTarget, 0.80f);
  squintTarget = qBound(0.0f, squintTarget, 0.60f);
  smileTarget = qBound(0.0f, smileTarget, 0.60f);

  m_browTarget = browTarget;
  m_cheekTarget = cheekTarget;
  m_squintTarget = squintTarget;
  m_smileTarget = smileTarget;

  m_browCurrent = approach(m_browCurrent, m_browTarget, 6.0f, dtMs);
  m_cheekCurrent = approach(m_cheekCurrent, m_cheekTarget, 6.0f, dtMs);
  m_squintCurrent = approach(m_squintCurrent, m_squintTarget, 6.0f, dtMs);
  m_smileCurrent = approach(m_smileCurrent, m_smileTarget, 6.0f, dtMs);

  std::fill(m_expressionWeights.begin(), m_expressionWeights.end(), 0.0f);

  auto set = [this](int index, float value) {
    if (index >= 0 &&
        index < static_cast<int>(m_expressionWeights.size())) {
      m_expressionWeights[static_cast<size_t>(index)] = value;
    }
  };

  set(m_browInnerLeft, m_browCurrent);
  set(m_browInnerRight, m_browCurrent);
  set(m_browOuterLeft, m_browCurrent * 0.6f);
  set(m_browOuterRight, m_browCurrent * 0.6f);

  set(m_cheekRaiseLeft, m_cheekCurrent);
  set(m_cheekRaiseRight, m_cheekCurrent);

  set(m_eyeSquintLeft, m_squintCurrent);
  set(m_eyeSquintRight, m_squintCurrent);

  set(m_mouthSmileLeft, m_smileCurrent);
  set(m_mouthSmileRight, m_smileCurrent);

  pushFaceWeights();
}

void AvatarController::pushFaceWeights() {
  if (m_morphWeights.empty()) {
    return;
  }

  const size_t n = m_morphWeights.size();

  for (size_t i = 0; i < n; ++i) {
    float v = 0.0f;

    if (i < m_visemeWeights.size()) {
      v += m_visemeWeights[i] * m_visemeGain;
    }

    if (i < m_expressionWeights.size()) {
      v += m_expressionWeights[i];
    }

    if (i < m_gazeWeights.size()) {
      v += m_gazeWeights[i];
    }

    if (i < m_blinkWeights.size()) {
      v += m_blinkWeights[i];
    }

    if (v > 1.0f) {
      v = 1.0f;
    }

    m_morphWeights[i] = v;
  }

  if (m_surface) {
    m_surface->setMorphWeights(m_morphWeights);
  }
}

void AvatarController::onTick() {
  constexpr int kTickMs = 16;

  updateBlink(kTickMs);
  updateGaze(kTickMs);
  updateExpression(kTickMs);

  if (m_currentClip.isEmpty()) {
    return;
  }

  updateJoints(0.016f);
  emit ticked();
}

void AvatarController::updateJoints(float dt) {
  if (!m_samplingContext) {
    return;
  }

  const auto clip = m_clips.value(m_currentClip);

  if (!clip) {
    return;
  }

  m_time += dt;

  if (m_clipDuration > 0.0f) {
    while (m_time >= m_clipDuration) {
      m_time -= m_clipDuration;
    }
  }

  ozz::animation::SamplingJob samplingJob;
  samplingJob.animation = clip.get();
  samplingJob.context = m_samplingContext.get();
  samplingJob.ratio =
      (m_clipDuration > 0.0f) ? (m_time / m_clipDuration) : 0.0f;
  samplingJob.output = ozz::span<ozz::math::SoaTransform>(
      m_localTransforms.data(), m_localTransforms.size());

  if (!samplingJob.Run()) {
    qWarning() << "[AvatarController] SamplingJob failed.";
    return;
  }

  ozz::animation::LocalToModelJob localToModelJob;
  localToModelJob.skeleton = &m_skeleton;
  localToModelJob.input = ozz::span<const ozz::math::SoaTransform>(
      m_localTransforms.data(), m_localTransforms.size());
  localToModelJob.output = ozz::span<ozz::math::Float4x4>(
      m_modelSpaceMatrices.data(), m_modelSpaceMatrices.size());

  if (!localToModelJob.Run()) {
    qWarning() << "[AvatarController] LocalToModelJob failed.";
    return;
  }

  for (int j = 0; j < m_skinToOzzJoint.size(); ++j) {
    const int ozzIndex = m_skinToOzzJoint.at(j);

    if (ozzIndex < 0) {
      m_skinningMatrices[static_cast<size_t>(j)].setToIdentity();
      continue;
    }

    const QMatrix4x4 modelSpace =
        toQMatrix4x4(m_modelSpaceMatrices[static_cast<size_t>(ozzIndex)]);

    m_skinningMatrices[static_cast<size_t>(j)] =
        modelSpace * m_inverseBindPoses.at(j);
  }

  if (m_surface) {
    m_surface->setSkinningMatrices(m_skinningMatrices);
  }
}