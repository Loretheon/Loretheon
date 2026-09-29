#include "../../include/assistant/MindMapView.h"

#include "../../include/assistant/MindMapNode.h"
#include "../../include/assistant/MindMapScene.h"

#include <QContextMenuEvent>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QRadialGradient>
#include <QResizeEvent>
#include <QScrollBar>
#include <QWheelEvent>

#include <cmath>

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
  setBackgroundBrush(Qt::NoBrush);

  if (m_scene) {
    connect(m_scene, &MindMapScene::openRequested, this,
            &MindMapView::openRequested);
    connect(m_scene, &MindMapScene::sessionOpenRequested, this,
            &MindMapView::sessionOpenRequested);
  }
}

void MindMapView::refresh() {
  if (!m_scene) {
    return;
  }

  m_hovered = nullptr;
  m_dragging = nullptr;
  m_panning = false;
  m_zoomStep = 0;

  resetTransform();

  if (!m_scene->sceneRect().isEmpty()) {
    centerOn(m_scene->sceneRect().center());
  }

  m_scene->clearFocus();
}

void MindMapView::mouseMoveEvent(QMouseEvent *event) {
  if (m_dragging) {
    const QPointF scenePos = mapToScene(event->pos());
    m_scene->pinNode(m_dragging, scenePos - m_dragOffset);
    event->accept();
    return;
  }

  if (m_panning) {
    const QPoint delta = event->pos() - m_panStart;
    m_panStart = event->pos();

    horizontalScrollBar()->setValue(horizontalScrollBar()->value() -
                                    delta.x());
    verticalScrollBar()->setValue(verticalScrollBar()->value() -
                                  delta.y());

    event->accept();
    return;
  }

  QGraphicsItem *item = itemAt(event->pos());

  MindMapNode *node = nullptr;

  if (item) {
    node = qgraphicsitem_cast<MindMapNode *>(item);
  }

  if (node != m_hovered) {
    m_hovered = node;
    applyHover(node);
  }

  QGraphicsView::mouseMoveEvent(event);
}

void MindMapView::mousePressEvent(QMouseEvent *event) {
  if (event->button() == Qt::LeftButton) {
    QGraphicsItem *item = itemAt(event->pos());

    if (item) {
      auto *node = qgraphicsitem_cast<MindMapNode *>(item);

      if (node) {
        m_dragging = node;
        m_dragOffset = mapToScene(event->pos()) - node->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
      }
    }

    m_panning = true;
    m_panStart = event->pos();
    setCursor(Qt::ClosedHandCursor);
    event->accept();
    return;
  }

  QGraphicsView::mousePressEvent(event);
}

void MindMapView::mouseReleaseEvent(QMouseEvent *event) {
  if (m_dragging && event->button() == Qt::LeftButton) {
    m_dragging = nullptr;
    unsetCursor();

    if (m_scene) {
      m_scene->saveLayout();
    }

    event->accept();
    return;
  }

  if (m_panning && event->button() == Qt::LeftButton) {
    m_panning = false;
    unsetCursor();
    event->accept();
    return;
  }

  QGraphicsView::mouseReleaseEvent(event);
}

void MindMapView::leaveEvent(QEvent *event) {
  m_hovered = nullptr;

  if (m_scene) {
    m_scene->clearFocus();
  }

  QGraphicsView::leaveEvent(event);
}

void MindMapView::wheelEvent(QWheelEvent *event) {
  const int steps = event->angleDelta().y() / 120;

  if (steps == 0) {
    QGraphicsView::wheelEvent(event);
    return;
  }

  m_zoomStep = qBound(-10, m_zoomStep + steps, 12);

  const qreal factor = std::pow(1.15, steps);

  scale(factor, factor);

  event->accept();
}

void MindMapView::resizeEvent(QResizeEvent *event) {
  QGraphicsView::resizeEvent(event);
}

void MindMapView::contextMenuEvent(QContextMenuEvent *event) {
  if (!m_scene) {
    return;
  }

  QMenu menu(this);

  QAction *refreshAction = menu.addAction(tr("Refresh"));
  menu.addSeparator();
  QAction *relayout = menu.addAction(tr("Re-layout graph"));
  QAction *fit = menu.addAction(tr("Fit to view"));

  QAction *chosen = menu.exec(event->globalPos());

  if (chosen == refreshAction) {
    emit refreshRequested();
  } else if (chosen == relayout) {
    m_scene->rebuildLayout();
    refresh();
  } else if (chosen == fit) {
    if (!m_scene->sceneRect().isEmpty()) {
      fitInView(m_scene->sceneRect(), Qt::KeepAspectRatio);
    }
  }
}

void MindMapView::applyHover(MindMapNode *node) {
  if (!m_scene) {
    return;
  }

  if (!node) {
    m_scene->clearFocus();
    return;
  }

  m_scene->focusOn(node);
}

void MindMapView::drawBackground(QPainter *painter, const QRectF &rect) {
  Q_UNUSED(rect);

  painter->save();
  painter->setRenderHint(QPainter::Antialiasing, true);

  const QPointF center = mapToScene(viewport()->rect().center());

  const QRectF sceneRect = this->sceneRect();
  const qreal radius =
      std::max(sceneRect.width(), sceneRect.height()) * 0.7;

  QRadialGradient gradient(center, radius);

  const QColor inner = palette().color(QPalette::Window).lighter(112);
  const QColor outer = palette().color(QPalette::Window).darker(112);

  gradient.setColorAt(0.0, inner);
  gradient.setColorAt(1.0, outer);

  painter->fillRect(sceneRect, gradient);

  painter->restore();
}