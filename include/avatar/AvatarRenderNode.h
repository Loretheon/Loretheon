#pragma once

#include "AvatarMeshData.h"

#include <QMatrix4x4>
#include <QPointF>
#include <QSizeF>
#include <QSGRenderNode>

#include <vector>

class AvatarRenderer;
class QQuickWindow;

class AvatarRenderNode : public QSGRenderNode {
public:
  explicit AvatarRenderNode(QQuickWindow *window = nullptr);
  ~AvatarRenderNode() override;

  void setMeshData(const AvatarMeshData &data);
  void setSkinningMatrices(const std::vector<QMatrix4x4> &matrices);
  void setMorphWeights(const std::vector<float> &weights);
  void setViewProjection(const QMatrix4x4 &vp);
  void setItemOrigin(const QPointF &origin);
  void setItemSize(const QSizeF &size);

  void render(const RenderState *state) override;
  void releaseResources() override;
  StateFlags changedStates() const override;

private:
  AvatarRenderer *m_renderer = nullptr;
  QQuickWindow *m_window = nullptr;

  AvatarMeshData m_meshData;
  std::vector<QMatrix4x4> m_skinningMatrices;
  std::vector<float> m_morphWeights;
  QMatrix4x4 m_viewProjection;
  QPointF m_itemOrigin;
  QSizeF m_itemSize;

  bool m_skinningDirty = true;
  bool m_morphDirty = true;

  bool m_initialized = false;
};