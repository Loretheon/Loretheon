#pragma once

#include "AvatarMeshData.h"

#include <QMatrix4x4>
#include <QSGRenderNode>
#include <QQuickWindow>

#include <vector>

class AvatarRenderer;

class AvatarRenderNode : public QSGRenderNode {
public:
  explicit AvatarRenderNode(QQuickWindow *window = nullptr);
  ~AvatarRenderNode() override;

  void setMeshData(const AvatarMeshData &data);
  void setSkinningMatrices(const std::vector<QMatrix4x4> &matrices);
  void setMorphWeights(const std::vector<float> &weights);
  void setViewProjection(const QMatrix4x4 &vp);

  void render(const RenderState *state) override;
  void releaseResources() override;
  StateFlags changedStates() const override;

private:
  AvatarRenderer *m_renderer = nullptr;

  AvatarMeshData m_meshData;
  std::vector<QMatrix4x4> m_skinningMatrices;
  std::vector<float> m_morphWeights;
  QMatrix4x4 m_viewProjection;

  // The renderer is pushed only when these change.
  bool m_skinningDirty = true;
  bool m_morphDirty = true;

  bool m_initialized = false;
};