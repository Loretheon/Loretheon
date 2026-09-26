#pragma once

#include <QGraphicsView>
#include <QPointF>
#include <QTimer>

class MindMapScene;
class MindMapNode;

class MindMapView : public QGraphicsView {
  Q_OBJECT

public:
  explicit MindMapView(MindMapScene *scene, QWidget *parent = nullptr);

  void refresh();

protected:
  void mouseMoveEvent(QMouseEvent *event) override;
  void leaveEvent(QEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;

private slots:
  void onEdgeTimer();
  void onNodeHovered(MindMapNode *node);

private:
  enum class Edge { None, Left, Right };

  Edge edgeForPosition(const QPointF &pos) const;

  void setEdge(Edge edge);
  void applyFocus(MindMapNode *node);

  MindMapScene *m_scene = nullptr;

  QTimer *m_edgeTimer = nullptr;

  Edge m_pendingEdge = Edge::None;
  Edge m_activeEdge = Edge::None;

  MindMapNode *m_hovered = nullptr;

  int m_zoomStep = 0;

  static constexpr int kEdgeBandPx = 48;
  static constexpr int kEdgeDelayMs = 220;
};