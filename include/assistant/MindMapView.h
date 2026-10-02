#pragma once

#include <QGraphicsView>
#include <QPoint>
#include <QPointF>
#include <QString>
#include <QStringList>

class QMimeData;
class LoreAssistant;
class MarkdownView;
class MindMapScene;
class MindMapNode;

class QScrollArea;
class QToolButton;

class MindMapView : public QGraphicsView {
  Q_OBJECT

public:
  explicit MindMapView(MindMapScene *scene, QWidget *parent = nullptr);
  ~MindMapView() override;

  void refresh();

  void setAssistant(LoreAssistant *assistant);
  LoreAssistant *assistant() const { return m_assistant; }

signals:
  void openRequested(const QString &path);
  void sessionOpenRequested(const QString &sessionName);
  void refreshRequested();

protected:
  void mouseMoveEvent(QMouseEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  void leaveEvent(QEvent *event) override;
  void wheelEvent(QWheelEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void drawBackground(QPainter *painter, const QRectF &rect) override;
  void contextMenuEvent(QContextMenuEvent *event) override;
  void dragEnterEvent(QDragEnterEvent *event) override;
  void dragMoveEvent(QDragMoveEvent *event) override;
  void dragLeaveEvent(QDragLeaveEvent *event) override;
  void dropEvent(QDropEvent *event) override;

private:
  void applyHover(MindMapNode *node);

  void openPreviewFor(MindMapNode *node);
  void closePreview();
  void positionPreviewFor(MindMapNode *node);
  void ensurePreview();

  bool canAcceptDrag(const QMimeData *mime) const;
  QStringList droppedPaths(const QMimeData *mime) const;

  MindMapScene *m_scene = nullptr;
  LoreAssistant *m_assistant = nullptr;

  MindMapNode *m_hovered = nullptr;
  MindMapNode *m_dragging = nullptr;

  bool m_panning = false;
  QPoint m_panStart;

  QPointF m_dragOffset;

  int m_zoomStep = 0;

  QScrollArea *m_previewPanel = nullptr;
  MarkdownView *m_preview = nullptr;
  QToolButton *m_previewClose = nullptr;
  MindMapNode *m_previewNode = nullptr;

  bool m_dropActive = false;
};