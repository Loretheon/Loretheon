#include "../../include/avatar/AvatarRenderNode.h"

#include "../../include/avatar/AvatarRenderer.h"

#include <QDebug>
#include <QQuickWindow>

AvatarRenderNode::AvatarRenderNode(QQuickWindow *window)
    : m_renderer(new AvatarRenderer()) {
  Q_UNUSED(window);
}

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

  m_renderer->render(m_viewProjection);
}

void AvatarRenderNode::releaseResources() {
  if (m_renderer) {
    m_renderer->destroy();
  }

  m_initialized = false;
}

QSGRenderNode::StateFlags AvatarRenderNode::changedStates() const {
  return DepthState | CullState | BlendState | ViewportState;
}