#include "../../include/avatar/AvatarRenderNode.h"

#include "../../include/avatar/AvatarRenderer.h"

#include <QDebug>
#include <QOpenGLFunctions>
#include <QQuickWindow>

AvatarRenderNode::AvatarRenderNode(QQuickWindow *window)
    : m_renderer(new AvatarRenderer()), m_window(window) {}

AvatarRenderNode::~AvatarRenderNode() { delete m_renderer; }

void AvatarRenderNode::setMeshData(const AvatarMeshData &data) {
  m_meshData = data;
  m_initialized = false;
}

void AvatarRenderNode::setSkinningMatrices(
    const std::vector<QMatrix4x4> &matrices) {
  m_skinningMatrices = matrices;
  m_skinningDirty = true;
}

void AvatarRenderNode::setMorphWeights(const std::vector<float> &weights) {
  m_morphWeights = weights;
  m_morphDirty = true;
}

void AvatarRenderNode::setViewProjection(const QMatrix4x4 &vp) {
  m_viewProjection = vp;
}

void AvatarRenderNode::setItemOrigin(const QPointF &origin) {
  m_itemOrigin = origin;
}

void AvatarRenderNode::setItemSize(const QSizeF &size) {
  m_itemSize = size;
}

void AvatarRenderNode::render(const RenderState *state) {
  Q_UNUSED(state);

  if (!m_initialized) {
    if (!m_renderer->initialize(m_meshData)) {
      qWarning() << "[AvatarRenderNode] Renderer initialization failed.";
      return;
    }

    m_initialized = true;
    m_skinningDirty = true;
    m_morphDirty = true;

    qDebug() << "[AvatarRenderNode] Renderer initialized.";
  }

  if (m_skinningDirty) {
    m_renderer->setSkinningMatrices(m_skinningMatrices);
    m_skinningDirty = false;
  }

  if (m_morphDirty) {
    m_renderer->setMorphWeights(m_morphWeights);
    m_morphDirty = false;
  }

  if (m_window && m_itemSize.isValid()) {
    const qreal dpr = m_window->devicePixelRatio();

    const int vw = qRound(m_itemSize.width() * dpr);
    const int vh = qRound(m_itemSize.height() * dpr);

    if (vw > 0 && vh > 0) {
      glViewport(0, 0, vw, vh);
    }
  }

  m_renderer->render(m_viewProjection);
}

void AvatarRenderNode::releaseResources() {
  if (m_renderer) {
    m_renderer->destroy();
  }

  m_initialized = false;
}

QSGRenderNode::StateFlags AvatarRenderNode::changedStates() const {
  return ViewportState;
}