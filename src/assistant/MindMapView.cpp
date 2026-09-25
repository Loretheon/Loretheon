#include "../../include/assistant/MindMapView.h"

#include "../../include/assistant/MindMapNode.h"
#include "../../include/assistant/MindMapScene.h"

#include <QMouseEvent>
#include <QResizeEvent>
#include <QScrollBar>
#include <QTimer>
#include <QWheelEvent>

MindMapView::MindMapView(MindMapScene *scene, QWidget *parent)
    : QGraphicsView(parent), m_scene(scene) {
  setScene(scene);
  setRenderHint(QPainter::Antialiasing, true);
  setRenderHint(QPainter::TextAntialiasing, true);
  setFrameShape(QFrame::NoFrame);
  setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
  setViewportUpdateMode(QGraphicsView::FullViewportUpdate);
  setDragMode(QGraphicsView::NoDrag);
  setInteractive(false);
  setMouseTracking(true);

  m_edgeTimer = new QTimer(this);
  m_edgeTimer->setSingleShot(true);
  m_edgeTimer->setInterval(kEdgeDelayMs);

  connect(m_edgeTimer, &QTimer::timeout, this,
          &MindMapView::onEdgeTimer);

  if (m_scene) {
    connect(m_scene, &MindMapScene::nodeHovered, this,
            &MindMapView::onNodeHovered);
  }
}

void MindMapView::refresh() {
  if (!m_scene) {
    return;
  }

  m_hovered = nullptr;

  m_scene->setVisibleDepth(3);

  centerOn(m_scene->sceneRect().center());
}

void MindMapView::mouseMoveEvent(QMouseEvent *event) {
  const QPointF local = event->position();
  const Edge edge = edgeForPosition(local);

  if (edge == Edge::None) {
    if (m_pendingEdge != Edge::None) {
      m_pendingEdge = Edge::None;
      m_edgeTimer->stop();
    }
  } else if (edge != m_pendingEdge) {
    m_pendingEdge = edge;
    m_edgeTimer->start();
  }

  QGraphicsView::mouseMoveEvent(event);
}

void MindMapView::leaveEvent(QEvent *event) {
  m_pendingEdge = Edge::None;
  m_activeEdge = Edge::None;
  m_edgeTimer->stop();

  if (m_scene) {
    m_scene->clearFocus();
  }

  m_hovered = nullptr;

  QGraphicsView::leaveEvent(event);
}

void MindMapView::wheelEvent(QWheelEvent *event) {
  const int steps = event->angleDelta().y() / 120;

  if (steps == 0) {
    QGraphicsView::wheelEvent(event);
    return;
  }

  const int newStep = qBound(-4, m_zoomStep + steps, 4);

  if (newStep != m_zoomStep) {
    m_zoomStep = newStep;

    const qreal factor = std::pow(1.15, m_zoomStep);

    resetTransform();
    scale(factor, factor);
  }

  event->accept();
}

void MindMapView::resizeEvent(QResizeEvent *event) {
  QGraphicsView::resizeEvent(event);

  if (m_scene) {
    m_scene->layoutTree();
  }
}

MindMapView::Edge MindMapView::edgeForPosition(const QPointF &pos) const {
  if (pos.x() < kEdgeBandPx) {
    return Edge::Left;
  }

  if (pos.x() > width() - kEdgeBandPx) {
    return Edge::Right;
  }

  return Edge::None;
}

void MindMapView::onEdgeTimer() {
  const Edge edge = m_pendingEdge;

  if (edge == Edge::None) {
    return;
  }

  m_activeEdge = edge;

  if (!m_scene) {
    return;
  }

  const int current = m_scene->visibleDepth();

  if (edge == Edge::Right) {
    m_scene->setVisibleDepth(current + 1);
  } else {
    m_scene->setVisibleDepth(current - 1);
  }

  m_scene->clearFocus();
  m_hovered = nullptr;
}

void MindMapView::onNodeHovered(MindMapNode *node) {
  Q_UNUSED(node);
}