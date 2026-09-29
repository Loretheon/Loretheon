#pragma once

#include "AvatarMeshData.h"

#include <QMatrix4x4>
#include <QMutex>
#include <QQuickItem>
#include <QTimer>

#include <memory>
#include <vector>

class AvatarRenderer;
class AvatarRenderNode;
class QQuickWindow;
class QSGNode;
struct UpdatePaintNodeData;

class AvatarSurface : public QQuickItem {
  Q_OBJECT

  Q_PROPERTY(float cameraDistance READ cameraDistance
                 WRITE setCameraDistance NOTIFY cameraChanged)
  Q_PROPERTY(float cameraYaw READ cameraYaw
                 WRITE setCameraYaw NOTIFY cameraChanged)
  Q_PROPERTY(float cameraPitch READ cameraPitch
                 WRITE setCameraPitch NOTIFY cameraChanged)
  Q_PROPERTY(float cameraFov READ cameraFov
                 WRITE setCameraFov NOTIFY cameraChanged)

  Q_PROPERTY(float targetX READ targetX WRITE setTargetX
                 NOTIFY cameraChanged)
  Q_PROPERTY(float targetY READ targetY WRITE setTargetY
                 NOTIFY cameraChanged)
  Q_PROPERTY(float targetZ READ targetZ WRITE setTargetZ
                 NOTIFY cameraChanged)

  Q_PROPERTY(float modelScale READ modelScale
                 WRITE setModelScale NOTIFY cameraChanged)
  Q_PROPERTY(float modelYaw READ modelYaw
                 WRITE setModelYaw NOTIFY cameraChanged)
  Q_PROPERTY(float modelPitch READ modelPitch
                 WRITE setModelPitch NOTIFY cameraChanged)
  Q_PROPERTY(float modelRoll READ modelRoll
                 WRITE setModelRoll NOTIFY cameraChanged)

public:
  explicit AvatarSurface(QQuickItem *parent = nullptr);
  ~AvatarSurface() override;

  void setMeshData(const AvatarMeshData &meshData);
  void setSkinningMatrices(const std::vector<QMatrix4x4> &matrices);
  void setMorphWeights(const std::vector<float> &weights);
  void setViewProjection(const QMatrix4x4 &viewProjection);

  bool isReady() const { return m_rendererReady; }

  float cameraDistance() const { return m_cameraDistance; }
  void setCameraDistance(float v);

  float cameraYaw() const { return m_cameraYaw; }
  void setCameraYaw(float v);

  float cameraPitch() const { return m_cameraPitch; }
  void setCameraPitch(float v);

  float cameraFov() const { return m_cameraFov; }
  void setCameraFov(float v);

  float targetX() const { return m_targetX; }
  void setTargetX(float v);

  float targetY() const { return m_targetY; }
  void setTargetY(float v);

  float targetZ() const { return m_targetZ; }
  void setTargetZ(float v);

  float modelScale() const { return m_modelScale; }
  void setModelScale(float v);

  float modelYaw() const { return m_modelYaw; }
  void setModelYaw(float v);

  float modelPitch() const { return m_modelPitch; }
  void setModelPitch(float v);

  float modelRoll() const { return m_modelRoll; }
  void setModelRoll(float v);

  Q_INVOKABLE void orbitBy(float dx, float dy);
  Q_INVOKABLE void panBy(float dx, float dy);
  Q_INVOKABLE void zoomBy(float steps);
  Q_INVOKABLE void resetCamera();

  void componentComplete() override;

signals:
  void ready();
  void cameraChanged();

private slots:
  void onFrameTimer();

protected:
  void geometryChange(const QRectF &newGeometry,
                      const QRectF &oldGeometry) override;
  QSGNode *updatePaintNode(QSGNode *oldNode,
                           UpdatePaintNodeData *updatePaintNodeData) override;

private:
  void rebuildProjection();
  void applyDefaults();

  AvatarRenderNode *m_node = nullptr;

  AvatarMeshData m_pendingMeshData;
  bool m_meshDataPending = false;
  QMutex m_meshMutex;

  std::vector<QMatrix4x4> m_skinningMatrices;
  std::vector<float> m_morphWeights;
  QMatrix4x4 m_viewProjection;
  QMutex m_matrixMutex;

  float m_cameraDistance = 2.42f;
  float m_cameraYaw = 0.0f;
  float m_cameraPitch = 0.0f;
  float m_cameraFov = 45.0f;

  float m_targetX = 0.0f;
  float m_targetY = 0.853f;
  float m_targetZ = 0.01f;

  float m_modelScale = 1.0f;
  float m_modelYaw = 0.0f;
  float m_modelPitch = 0.0f;
  float m_modelRoll = 0.0f;

  float m_defaultCameraDistance = 2.42f;
  float m_defaultCameraYaw = 0.0f;
  float m_defaultCameraPitch = 0.0f;
  float m_defaultTargetX = 0.0f;
  float m_defaultTargetY = 0.853f;
  float m_defaultTargetZ = 0.01f;

  QTimer *m_frameTimer = nullptr;

  bool m_rendererReady = false;
};