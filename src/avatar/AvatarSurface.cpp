#include "../../include/avatar/AvatarSurface.h"

#include "../../include/avatar/AvatarRenderNode.h"

#include <QDebug>
#include <QQuickWindow>
#include <QSGNode>

#include <cmath>

namespace {

constexpr float kOrbitDegreesPerPixel = 0.4f;
constexpr float kZoomFactorPerNotch = 0.9f;
constexpr float kPanFractionPerPixel = 0.0035f;

float clampPitch(float pitch) {
  return qBound(-89.0f, pitch, 89.0f);
}

} // namespace

AvatarSurface::AvatarSurface(QQuickItem *parent) : QQuickItem(parent) {
  setFlag(QQuickItem::ItemHasContents, true);

  applyDefaults();
  rebuildProjection();

  m_frameTimer = new QTimer(this);
  m_frameTimer->setInterval(16);
  connect(m_frameTimer, &QTimer::timeout, this,
          &AvatarSurface::onFrameTimer);
}

AvatarSurface::~AvatarSurface() {
  if (m_frameTimer) {
    m_frameTimer->stop();
  }
}

void AvatarSurface::applyDefaults() {
  m_defaultCameraDistance = m_cameraDistance;
  m_defaultCameraYaw = m_cameraYaw;
  m_defaultCameraPitch = m_cameraPitch;
  m_defaultTargetX = m_targetX;
  m_defaultTargetY = m_targetY;
  m_defaultTargetZ = m_targetZ;
}

void AvatarSurface::resetCamera() {
  m_cameraDistance = m_defaultCameraDistance;
  m_cameraYaw = m_defaultCameraYaw;
  m_cameraPitch = m_defaultCameraPitch;
  m_targetX = m_defaultTargetX;
  m_targetY = m_defaultTargetY;
  m_targetZ = m_defaultTargetZ;
  rebuildProjection();
  emit cameraChanged();
}

void AvatarSurface::componentComplete() {
  QQuickItem::componentComplete();
  applyDefaults();
  rebuildProjection();
}

void AvatarSurface::onFrameTimer() { update(); }

void AvatarSurface::geometryChange(const QRectF &newGeometry,
                                   const QRectF &oldGeometry) {
  QQuickItem::geometryChange(newGeometry, oldGeometry);
  rebuildProjection();
}

void AvatarSurface::setCameraDistance(float v) {
  if (qFuzzyCompare(m_cameraDistance, v)) return;
  m_cameraDistance = qMax(0.1f, v);
  rebuildProjection();
  emit cameraChanged();
}

void AvatarSurface::setCameraYaw(float v) {
  if (qFuzzyCompare(m_cameraYaw, v)) return;
  m_cameraYaw = v;
  rebuildProjection();
  emit cameraChanged();
}

void AvatarSurface::setCameraPitch(float v) {
  v = clampPitch(v);
  if (qFuzzyCompare(m_cameraPitch, v)) return;
  m_cameraPitch = v;
  rebuildProjection();
  emit cameraChanged();
}

void AvatarSurface::setCameraFov(float v) {
  if (qFuzzyCompare(m_cameraFov, v)) return;
  m_cameraFov = qBound(5.0f, v, 120.0f);
  rebuildProjection();
  emit cameraChanged();
}

void AvatarSurface::setTargetX(float v) {
  if (qFuzzyCompare(m_targetX, v)) return;
  m_targetX = v;
  rebuildProjection();
  emit cameraChanged();
}

void AvatarSurface::setTargetY(float v) {
  if (qFuzzyCompare(m_targetY, v)) return;
  m_targetY = v;
  rebuildProjection();
  emit cameraChanged();
}

void AvatarSurface::setTargetZ(float v) {
  if (qFuzzyCompare(m_targetZ, v)) return;
  m_targetZ = v;
  rebuildProjection();
  emit cameraChanged();
}

void AvatarSurface::setModelScale(float v) {
  if (qFuzzyCompare(m_modelScale, v)) return;
  m_modelScale = v;
  rebuildProjection();
  emit cameraChanged();
}

void AvatarSurface::setModelYaw(float v) {
  if (qFuzzyCompare(m_modelYaw, v)) return;
  m_modelYaw = v;
  rebuildProjection();
  emit cameraChanged();
}

void AvatarSurface::setModelPitch(float v) {
  if (qFuzzyCompare(m_modelPitch, v)) return;
  m_modelPitch = v;
  rebuildProjection();
  emit cameraChanged();
}

void AvatarSurface::setModelRoll(float v) {
  if (qFuzzyCompare(m_modelRoll, v)) return;
  m_modelRoll = v;
  rebuildProjection();
  emit cameraChanged();
}

void AvatarSurface::orbitBy(float dx, float dy) {
  m_cameraYaw += dx * kOrbitDegreesPerPixel;
  m_cameraPitch = clampPitch(m_cameraPitch + dy * kOrbitDegreesPerPixel);
  rebuildProjection();
  emit cameraChanged();
}

void AvatarSurface::panBy(float dx, float dy) {
  const float yawRad = qDegreesToRadians(m_cameraYaw);
  const float pitchRad = qDegreesToRadians(m_cameraPitch);

  const QVector3D forward(
      -std::sin(yawRad) * std::cos(pitchRad),
      -std::sin(pitchRad),
      -std::cos(yawRad) * std::cos(pitchRad));

  const QVector3D worldUp(0.0f, 1.0f, 0.0f);

  QVector3D right = QVector3D::crossProduct(forward, worldUp);
  if (right.lengthSquared() < 1e-6f) {
    right = QVector3D(1.0f, 0.0f, 0.0f);
  } else {
    right.normalize();
  }

  const QVector3D up = QVector3D::crossProduct(right, forward).normalized();

  const float scale = m_cameraDistance * kPanFractionPerPixel;

  const QVector3D shift =
      right * (-dx * scale) +
      up * (dy * scale);

  m_targetX += shift.x();
  m_targetY += shift.y();
  m_targetZ += shift.z();

  rebuildProjection();
  emit cameraChanged();
}

void AvatarSurface::zoomBy(float steps) {
  m_cameraDistance *= std::pow(kZoomFactorPerNotch, steps);

  if (m_cameraDistance < 0.1f) m_cameraDistance = 0.1f;
  if (m_cameraDistance > 1000.0f) m_cameraDistance = 1000.0f;

  rebuildProjection();
  emit cameraChanged();
}

void AvatarSurface::rebuildProjection() {
  const float w = static_cast<float>(width());
  const float h = static_cast<float>(height());

  if (w <= 0.0f || h <= 0.0f) {
    return;
  }

  QMatrix4x4 projection;
  projection.perspective(m_cameraFov, w / h, 0.01f, 1000.0f);

  const float yawRad = qDegreesToRadians(m_cameraYaw);
  const float pitchRad = qDegreesToRadians(m_cameraPitch);

  const float cosPitch = std::cos(pitchRad);
  const float sinPitch = std::sin(pitchRad);
  const float sinYaw = std::sin(yawRad);
  const float cosYaw = std::cos(yawRad);

  const QVector3D eyeOffset(
      m_cameraDistance * cosPitch * sinYaw,
      m_cameraDistance * sinPitch,
      m_cameraDistance * cosPitch * cosYaw);

  const QVector3D target(m_targetX, m_targetY, m_targetZ);
  const QVector3D eye = target + eyeOffset;

  QMatrix4x4 view;
  view.lookAt(eye, target, QVector3D(0.0f, 1.0f, 0.0f));

  QMatrix4x4 model;
  model.rotate(m_modelPitch, 1.0f, 0.0f, 0.0f);
  model.rotate(m_modelYaw, 0.0f, 1.0f, 0.0f);
  model.rotate(m_modelRoll, 0.0f, 0.0f, 1.0f);
  model.scale(m_modelScale);

  QMutexLocker locker(&m_matrixMutex);
  m_viewProjection = projection * view * model;

  qDebug() << "[AvatarSurface] rebuild:"
           << "w:" << w << "h:" << h
           << "aspect:" << (w / h)
           << "dist:" << m_cameraDistance
           << "target:" << m_targetX << m_targetY << m_targetZ;
}void AvatarSurface::setMeshData(const AvatarMeshData &meshData) {
  QMutexLocker locker(&m_meshMutex);
  m_pendingMeshData = meshData;
  m_meshDataPending = true;
}

void AvatarSurface::setSkinningMatrices(
    const std::vector<QMatrix4x4> &matrices) {
  QMutexLocker locker(&m_matrixMutex);
  m_skinningMatrices = matrices;
}

void AvatarSurface::setMorphWeights(const std::vector<float> &weights) {
  QMutexLocker locker(&m_matrixMutex);
  m_morphWeights = weights;
}

void AvatarSurface::setViewProjection(const QMatrix4x4 &viewProjection) {
  QMutexLocker locker(&m_matrixMutex);
  m_viewProjection = viewProjection;
}

QSGNode *AvatarSurface::updatePaintNode(
    QSGNode *oldNode, UpdatePaintNodeData *updatePaintNodeData) {
  Q_UNUSED(updatePaintNodeData);

  AvatarRenderNode *node = static_cast<AvatarRenderNode *>(oldNode);

  if (!node) {
    node = new AvatarRenderNode(window());
    m_node = node;

    if (!m_rendererReady) {
      m_rendererReady = true;

      // Rebuild the projection against the size the item actually
      // has now. The constructor ran before layout, when the size was
      // zero, so m_viewProjection has never been built with a real
      // aspect. Doing it here means the first frame the scene graph
      // draws is against the final size.
      rebuildProjection();

      emit ready();

      // Start the per-frame update only now. Before this point the
      // item had no size, so update() would be a no-op.
      m_frameTimer->start();
    }
  }

  AvatarMeshData meshData;
  {
    QMutexLocker locker(&m_meshMutex);
    if (m_meshDataPending) {
      meshData = m_pendingMeshData;
      m_meshDataPending = false;
    }
  }

  std::vector<QMatrix4x4> skinning;
  std::vector<float> morphWeights;
  QMatrix4x4 viewProjection;
  {
    QMutexLocker locker(&m_matrixMutex);
    skinning = m_skinningMatrices;
    morphWeights = m_morphWeights;
    viewProjection = m_viewProjection;
  }

  if (!meshData.meshes.isEmpty()) {
    node->setMeshData(meshData);
  }
  node->setSkinningMatrices(skinning);
  node->setMorphWeights(morphWeights);
  node->setViewProjection(viewProjection);

  return node;
}