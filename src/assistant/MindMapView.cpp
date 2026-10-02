#include "../../include/assistant/MindMapView.h"

#include "../../include/assistant/LoreAssistant.h"
#include "../../include/assistant/MindMapNode.h"
#include "../../include/assistant/MindMapScene.h"
#include "../../include/overseer/MarkdownView.h"

#include <QContextMenuEvent>
#include <QDragEnterEvent>
#include <QDragLeaveEvent>
#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFileInfo>
#include <QMenu>
#include <QMimeData>
#include <QMouseEvent>
#include <QPainter>
#include <QRadialGradient>
#include <QResizeEvent>
#include <QScrollArea>
#include <QScrollBar>
#include <QToolButton>
#include <QUrl>
#include <QWheelEvent>

#include <cmath>

namespace {

constexpr int kPreviewMargin = 12;
constexpr int kCloseSize = 24;

} // namespace

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
  setAcceptDrops(true);

  if (m_scene) {
    connect(m_scene, &MindMapScene::openRequested, this,
            &MindMapView::openRequested);
    connect(m_scene, &MindMapScene::sessionOpenRequested, this,
            &MindMapView::sessionOpenRequested);
  }
}

MindMapView::~MindMapView() {
  if (m_previewClose) {
    m_previewClose->hide();
    m_previewClose->deleteLater();
    m_previewClose = nullptr;
  }

  if (m_previewPanel) {
    m_previewPanel->hide();
    m_previewPanel->deleteLater();
    m_previewPanel = nullptr;
    m_preview = nullptr;
  }
}

void MindMapView::setAssistant(LoreAssistant *assistant) {
  m_assistant = assistant;
}

void MindMapView::refresh() {
  if (!m_scene) {
    return;
  }

  m_hovered = nullptr;
  m_dragging = nullptr;
  m_panning = false;
  m_zoomStep = 0;

  closePreview();

  resetTransform();

  if (!m_scene->sceneRect().isEmpty()) {
    centerOn(m_scene->sceneRect().center());
  }

  m_scene->clearFocus();
}

void MindMapView::ensurePreview() {
  if (m_previewPanel) {
    return;
  }

  // The panel is a sibling of the viewport, parented to the view
  // itself, not to the viewport. That way wheel events over the panel
  // are delivered to the scroll area and never reach the graphics
  // view, which would zoom the graph instead.

  m_previewPanel = new QScrollArea(this);
  m_previewPanel->setObjectName(QStringLiteral("mindMapPreviewPanel"));
  m_previewPanel->setFrameShape(QFrame::NoFrame);
  m_previewPanel->setWidgetResizable(true);
  m_previewPanel->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
  m_previewPanel->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
  m_previewPanel->hide();

  m_preview = new MarkdownView(m_previewPanel);
  m_preview->setObjectName(QStringLiteral("mindMapPreview"));
  m_preview->setFocusPolicy(Qt::NoFocus);
  m_preview->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

  m_previewPanel->setWidget(m_preview);

  m_previewClose = new QToolButton(this);
  m_previewClose->setObjectName(QStringLiteral("mindMapPreviewClose"));
  m_previewClose->setText(QStringLiteral("✕"));
  m_previewClose->setToolTip(tr("Close"));
  m_previewClose->setCursor(Qt::PointingHandCursor);
  m_previewClose->setFocusPolicy(Qt::NoFocus);
  m_previewClose->setAutoRaise(true);
  m_previewClose->setFixedSize(kCloseSize, kCloseSize);
  m_previewClose->hide();

  connect(m_previewClose, &QToolButton::clicked, this,
          [this]() { closePreview(); });
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

    if (m_previewPanel && m_previewPanel->isVisible()) {
      closePreview();
    }

    m_panning = true;
    m_panStart = event->pos();
    setCursor(Qt::ClosedHandCursor);
    event->accept();
    return;
  }

  if (event->button() == Qt::MiddleButton) {
    m_panning = true;
    m_panStart = event->pos();
    setCursor(Qt::ClosedHandCursor);
    event->accept();
    return;
  }

  QGraphicsView::mousePressEvent(event);
}

void MindMapView::mouseDoubleClickEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton) {
    QGraphicsView::mouseDoubleClickEvent(event);
    return;
  }

  QGraphicsItem *item = itemAt(event->pos());

  if (!item) {
    QGraphicsView::mouseDoubleClickEvent(event);
    return;
  }

  auto *node = qgraphicsitem_cast<MindMapNode *>(item);

  if (!node) {
    QGraphicsView::mouseDoubleClickEvent(event);
    return;
  }

  m_dragging = nullptr;
  unsetCursor();

  if (m_previewNode == node && m_previewPanel &&
      m_previewPanel->isVisible()) {
    closePreview();
  } else {
    openPreviewFor(node);
  }

  event->accept();
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

  if (m_panning && (event->button() == Qt::LeftButton ||
                    event->button() == Qt::MiddleButton)) {
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

  if (m_previewNode) {
    positionPreviewFor(m_previewNode);
  }

  event->accept();
}

void MindMapView::resizeEvent(QResizeEvent *event) {
  QGraphicsView::resizeEvent(event);

  if (m_previewNode) {
    positionPreviewFor(m_previewNode);
  }
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

void MindMapView::openPreviewFor(MindMapNode *node) {
  if (!node) {
    closePreview();
    return;
  }

  const QString body = node->detail();

  if (body.trimmed().isEmpty()) {
    closePreview();
    return;
  }

  ensurePreview();

  QString markdown;

  if (node->kind() == MindMapNode::Kind::ProfileFile ||
      node->kind() == MindMapNode::Kind::MemoryTopic ||
      node->kind() == MindMapNode::Kind::MemorySession) {
    markdown = QStringLiteral("### ") + node->title() +
               QStringLiteral("\n\n") + body;
  } else {
    markdown = body;
  }

  m_preview->setMarkdownText(markdown);

  m_previewNode = node;

  positionPreviewFor(node);

  m_previewPanel->show();
  m_previewPanel->raise();

  if (m_previewClose) {
    m_previewClose->show();
    m_previewClose->raise();
  }
}

void MindMapView::closePreview() {
  if (m_previewPanel) {
    m_previewPanel->hide();
  }

  if (m_previewClose) {
    m_previewClose->hide();
  }

  m_previewNode = nullptr;
}

void MindMapView::positionPreviewFor(MindMapNode *node) {
  if (!node || !m_previewPanel) {
    return;
  }

  const QRectF nodeScene = node->sceneBoundingRect();
  const QRect nodeView = mapFromScene(nodeScene).boundingRect();

  const int viewWidth = width();
  const int viewHeight = height();

  const int half = viewWidth / 2;

  const bool nodeOnRight = nodeView.center().x() >= half;

  const int panelX = nodeOnRight ? 0 : half;

  const int panelWidth = half;

  const int panelY = 0;
  const int panelHeight = viewHeight;

  m_previewPanel->setGeometry(panelX, panelY,
                              qMax(120, panelWidth),
                              qMax(120, panelHeight));

  if (m_previewClose) {
    const int closeX = panelX + panelWidth - kCloseSize - kPreviewMargin;
    const int closeY = kPreviewMargin;

    m_previewClose->move(closeX, closeY);
  }
}

bool MindMapView::canAcceptDrag(const QMimeData *mime) const {
  if (!mime || !mime->hasUrls()) {
    return false;
  }

  for (const QUrl &url : mime->urls()) {
    if (!url.isLocalFile()) {
      continue;
    }

    const QString path = url.toLocalFile();

    if (m_assistant && m_assistant->canImport(path)) {
      return true;
    }
  }

  return false;
}

QStringList MindMapView::droppedPaths(const QMimeData *mime) const {
  QStringList result;

  if (!mime || !mime->hasUrls()) {
    return result;
  }

  for (const QUrl &url : mime->urls()) {
    if (!url.isLocalFile()) {
      continue;
    }

    const QString path = url.toLocalFile();

    if (m_assistant && m_assistant->canImport(path)) {
      result.append(path);
    }
  }

  return result;
}

void MindMapView::dragEnterEvent(QDragEnterEvent *event) {
  if (canAcceptDrag(event->mimeData())) {
    m_dropActive = true;
    event->acceptProposedAction();
    return;
  }

  event->ignore();
}

void MindMapView::dragMoveEvent(QDragMoveEvent *event) {
  if (canAcceptDrag(event->mimeData())) {
    event->acceptProposedAction();
    return;
  }

  event->ignore();
}

void MindMapView::dragLeaveEvent(QDragLeaveEvent *event) {
  m_dropActive = false;
  event->accept();
}

void MindMapView::dropEvent(QDropEvent *event) {
  m_dropActive = false;

  if (!m_assistant) {
    event->ignore();
    return;
  }

  const QStringList paths = droppedPaths(event->mimeData());

  if (paths.isEmpty()) {
    event->ignore();
    return;
  }

  for (const QString &path : paths) {
    const QString topic = QFileInfo(path).completeBaseName();

    m_assistant->importToMemory(path, topic);
  }

  event->acceptProposedAction();
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