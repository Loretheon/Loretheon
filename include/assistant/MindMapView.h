#pragma once

#include <QGraphicsView>
#include <QPoint>
#include <QPointF>

class MindMapScene;
class MindMapNode;

class MindMapView : public QGraphicsView {
  Q_OBJECT

public:
  explicit MindMapView(MindMapScene *scene, QWidget *parent = nullptr);

  void refresh();

  signals:
    void openRequested(const QString &path);
  void sessionOpenRequested(const QString &sessionName);
  void refreshRequested();

protected:
  void mouseMoveEvent(QMouseEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void leaveEvent(QEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void drawBackground(QPainter *painter, const QRectF &rect) override;
  void contextMenuEvent(QContextMenuEvent *event) override;

private:
  void applyHover(MindMapNode *node);

  MindMapScene *m_scene = nullptr;

  MindMapNode *m_hovered = nullptr;
  MindMapNode *m_dragging = nullptr;

  bool m_panning = false;
  QPoint m_panStart;

  QPointF m_dragOffset;

  int m_zoomStep = 0;
};