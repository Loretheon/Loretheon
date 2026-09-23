#include "../../include/avatar/AvatarController.h"

#include <QDebug>
#include <QDir>
#include <QDirIterator>
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

bool AvatarController::loadArchives(const QString &skeletonPath,
                                    const QString &clipDir) {
  // QDir and QDirIterator do not agree on the resource path form.
  // QDir wants ":/avatar/ccbase". QDirIterator wants the URL form.
  // Normalize whatever the caller passed in to the QDir form.
  auto normalizeResourcePath = [](const QString &path) -> QString {
    if (path.startsWith(QStringLiteral("qrc:/"))) {
      return QStringLiteral(":") + path.mid(4);
    }
    return path;
  };

  const QString dirPath = normalizeResourcePath(clipDir);
  const QString skelPath = normalizeResourcePath(skeletonPath);

  // -----------------------------------------------------------------
  // Resource visibility.
  // -----------------------------------------------------------------

  {
    QDir dir(dirPath);

    if (!dir.exists()) {
      qWarning() << "[AvatarController] Resource directory does not exist:"
                 << dirPath;
      return false;
    }

    const QStringList ozzEntries =
        dir.entryList(QStringList{QStringLiteral("*.ozz")},
                      QDir::Files | QDir::Readable, QDir::Name);

    qDebug() << "[AvatarController] Resource root:" << dirPath;
    qDebug() << "[AvatarController] Found" << ozzEntries.size()
             << ".ozz files:";

    for (const QString &entry : ozzEntries) {
      qDebug() << "  " << entry;
    }

    if (ozzEntries.isEmpty()) {
      qWarning() << "[AvatarController] No .ozz files under the"
                 << "resource root. Check the .qrc and re-run CMake.";
      return false;
    }
  }

  // -----------------------------------------------------------------
  // Skeleton. Fatal if it fails.
  // -----------------------------------------------------------------

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

  // -----------------------------------------------------------------
  // Clips. Each failure logged individually.
  // -----------------------------------------------------------------

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
  if (!qmlRoot) {
    qWarning() << "[AvatarController] attachToScene: null root.";
    return false;
  }

  if (!m_skeletonLoaded) {
    qWarning() << "[AvatarController] attachToScene: no skeleton loaded.";
    return false;
  }

  QObject *modelRoot =
      qmlRoot->findChild<QObject *>(nodeName, Qt::FindChildrenRecursively);

  if (!modelRoot) {
    qWarning() << "[AvatarController] Could not find model root Node named"
               << nodeName << "under the QML root.";
    return false;
  }

  m_jointNodes.clear();
  m_jointNodes.reserve(m_jointNames.size());

  const QList<QObject *> children = modelRoot->children();

  for (const QString &jointName : std::as_const(m_jointNames)) {
    QObject *found = nullptr;

    for (QObject *child : children) {
      if (child->objectName() == jointName) {
        found = child;
        break;
      }
    }

    if (!found) {
      found = modelRoot->findChild<QObject *>(
          jointName, Qt::FindChildrenRecursively);
    }

    m_jointNodes.append(found);
  }

  int matched = 0;

  for (QObject *node : std::as_const(m_jointNodes)) {
    if (node) {
      ++matched;
    }
  }

  qDebug() << "[AvatarController] Matched" << matched << "of"
           << m_jointNames.size() << "joint names in the scene graph.";

  if (matched == 0) {
    qWarning() << "[AvatarController] No joint names matched. Dumping"
               << "first 20 children of the model root:";

    int shown = 0;

    for (QObject *child : children) {
      if (shown++ >= 20) {
        break;
      }

      qDebug() << "  child objectName:" << child->objectName()
               << "class:" << child->metaObject()->className();
    }

    return false;
  }

  const size_t jointCount = m_skeleton.num_joints();

  m_localTransforms.resize(jointCount);
  m_modelSpaceMatrices.resize(jointCount);

  m_attached = true;
  return true;
}

void AvatarController::start() {
  if (!m_attached) {
    qWarning() << "[AvatarController] start called before attachToScene.";
    return;
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

void AvatarController::onTick() {
  if (!m_attached || m_currentClip.isEmpty()) {
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

  for (int i = 0; i < m_jointNodes.size(); ++i) {
    QObject *node = m_jointNodes.at(i);

    if (!node) {
      continue;
    }

    const QMatrix4x4 m = toQMatrix4x4(m_modelSpaceMatrices[i]);

    const QVector3D translation = m.column(3).toVector3D();

    QVector3D scale;
    scale.setX(QVector3D(m.column(0)).length());
    scale.setY(QVector3D(m.column(1)).length());
    scale.setZ(QVector3D(m.column(2)).length());

    QMatrix3x3 rot3x3;
    rot3x3(0, 0) = m(0, 0) / scale.x();
    rot3x3(0, 1) = m(0, 1) / scale.y();
    rot3x3(0, 2) = m(0, 2) / scale.z();
    rot3x3(1, 0) = m(1, 0) / scale.x();
    rot3x3(1, 1) = m(1, 1) / scale.y();
    rot3x3(1, 2) = m(1, 2) / scale.z();
    rot3x3(2, 0) = m(2, 0) / scale.x();
    rot3x3(2, 1) = m(2, 1) / scale.y();
    rot3x3(2, 2) = m(2, 2) / scale.z();

    const QQuaternion rotation = QQuaternion::fromRotationMatrix(rot3x3);

    node->setProperty("position", QVariant::fromValue(translation));
    node->setProperty("rotation", QVariant::fromValue(rotation));
    node->setProperty("scale", QVariant::fromValue(scale));
  }
}