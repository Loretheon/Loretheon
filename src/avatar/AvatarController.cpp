#include "../../include/avatar/AvatarController.h"

#include "../../include/avatar/AvatarSurface.h"

#include "../../include/avatar/AvatarMeshLoader.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMatrix4x4>
#include <QQuickItem>
#include <QTimer>
#include <QVariant>

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

} // namespace

AvatarController::AvatarController(QObject *parent) : QObject(parent) {
  m_timer = new QTimer(this);
  m_timer->setInterval(16);
  connect(m_timer, &QTimer::timeout, this, &AvatarController::onTick);
}

AvatarController::~AvatarController() = default;

void AvatarController::setSurface(AvatarSurface *surface) {
  m_surface = surface;
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

  const AvatarPrimitive &prim = face.primitives.first();

  for (int t = 0; t < prim.morphTargetNames.size(); ++t) {
    const QString &name = prim.morphTargetNames.at(t);

    if (name.isEmpty()) {
      continue;
    }

    // First occurrence wins. All six primitives share the same order.
    if (!m_meshData.faceMorphNameToIndex.contains(name)) {
      m_meshData.faceMorphNameToIndex.insert(name, t);
    }
  }

  m_visemeTable.build(m_meshData.faceMorphNameToIndex);

  m_morphWeights.assign(
      static_cast<size_t>(prim.morphTargetCount()), 0.0f);

  qDebug() << "[AvatarController] Morph data:"
           << m_meshData.faceMorphNameToIndex.size() << "named targets,"
           << m_morphWeights.size() << "weight slots.";
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

  // One-shot bind-pose bounds check. The skinning matrices are
  // identity here, so skinned bounds should equal source bounds. A
  // difference means the skinning deforms the bind pose.
  {
    float srcMinX = 1e9f, srcMaxX = -1e9f;
    float srcMinY = 1e9f, srcMaxY = -1e9f;
    float srcMinZ = 1e9f, srcMaxZ = -1e9f;

    float dstMinX = 1e9f, dstMaxX = -1e9f;
    float dstMinY = 1e9f, dstMaxY = -1e9f;
    float dstMinZ = 1e9f, dstMaxZ = -1e9f;

    for (const AvatarMesh &mesh : m_meshData.meshes) {
      for (const AvatarPrimitive &prim : mesh.primitives) {
        for (int i = 0; i < prim.vertexCount(); ++i) {
          const float x = prim.positions.at(i * 3 + 0);
          const float y = prim.positions.at(i * 3 + 1);
          const float z = prim.positions.at(i * 3 + 2);

          srcMinX = qMin(srcMinX, x);
          srcMaxX = qMax(srcMaxX, x);
          srcMinY = qMin(srcMinY, y);
          srcMaxY = qMax(srcMaxY, y);
          srcMinZ = qMin(srcMinZ, z);
          srcMaxZ = qMax(srcMaxZ, z);

          const int j0 = prim.jointIndices.at(i * 4 + 0);
          const int j1 = prim.jointIndices.at(i * 4 + 1);
          const int j2 = prim.jointIndices.at(i * 4 + 2);
          const int j3 = prim.jointIndices.at(i * 4 + 3);

          const float w0 = prim.jointWeights.at(i * 4 + 0);
          const float w1 = prim.jointWeights.at(i * 4 + 1);
          const float w2 = prim.jointWeights.at(i * 4 + 2);
          const float w3 = prim.jointWeights.at(i * 4 + 3);

          QMatrix4x4 skin;
          for (int r = 0; r < 4; ++r) {
            for (int c = 0; c < 4; ++c) {
              skin(r, c) = 0.0f;
            }
          }

          auto add = [&](int j, float w) {
            if (j < 0 ||
                j >= static_cast<int>(m_skinningMatrices.size())) {
              return;
            }

            const QMatrix4x4 &m =
                m_skinningMatrices[static_cast<size_t>(j)];

            for (int r = 0; r < 4; ++r) {
              for (int c = 0; c < 4; ++c) {
                skin(r, c) += w * m(r, c);
              }
            }
          };

          add(j0, w0);
          add(j1, w1);
          add(j2, w2);
          add(j3, w3);

          const QVector4D p = skin * QVector4D(x, y, z, 1.0f);

          dstMinX = qMin(dstMinX, p.x());
          dstMaxX = qMax(dstMaxX, p.x());
          dstMinY = qMin(dstMinY, p.y());
          dstMaxY = qMax(dstMaxY, p.y());
          dstMinZ = qMin(dstMinZ, p.z());
          dstMaxZ = qMax(dstMaxZ, p.z());
        }
      }
    }

    qDebug() << "[AvatarController] Bind-pose bounds check:";
    qDebug() << "  source  y:" << srcMinY << ".." << srcMaxY
             << " x:" << srcMinX << ".." << srcMaxX
             << " z:" << srcMinZ << ".." << srcMaxZ;
    qDebug() << "  skinned y:" << dstMinY << ".." << dstMaxY
             << " x:" << dstMinX << ".." << dstMaxX
             << " z:" << dstMinZ << ".." << dstMaxZ;
  }

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
  qDebug() << "[AvatarController] applyViseme called with"
         << viseme
         << "table keys:" << m_visemeTable.knownVisemes();

  if (m_morphWeights.empty()) {
    return;
  }

  std::fill(m_morphWeights.begin(), m_morphWeights.end(), 0.0f);

  if (viseme != QStringLiteral("sil") && !viseme.isEmpty()) {
    const QVector<VisemeTable::Weight> weights =
        m_visemeTable.weightsFor(viseme);

    for (const VisemeTable::Weight &w : weights) {
      if (w.morphIndex < 0 ||
          w.morphIndex >= static_cast<int>(m_morphWeights.size())) {
        continue;
          }

      m_morphWeights[static_cast<size_t>(w.morphIndex)] = w.weight;
    }
  }

  int set = 0;
  for (float w : m_morphWeights) {
    if (w != 0.0f) {
      ++set;
    }
  }

  qDebug() << "[AvatarController] applyViseme" << viseme
           << "shapes applied:" << set
           << "of" << m_morphWeights.size();

  if (m_surface) {
    m_surface->setMorphWeights(m_morphWeights);
  }
}
void AvatarController::onTick() {
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