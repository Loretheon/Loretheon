#pragma once

#include <QPoint>
#include <QRect>
#include <QString>
#include <QWidget>

class QLabel;
class QPushButton;
class TextDocument;

class WorkstationWindow : public QWidget {
  Q_OBJECT

public:
  enum class ResizeEdge {
    None,
    Top,
    Bottom,
    Left,
    Right,
    TopLeft,
    TopRight,
    BottomLeft,
    BottomRight,
  };

  WorkstationWindow(TextDocument *document, QWidget *body,
                    const QString &filePath, QWidget *parent);

  ~WorkstationWindow() override;

  TextDocument *document() const { return m_document; }

  QString filePath() const { return m_filePath; }

  QString displayName() const;

  void setStatusPill(const QString &status);

  void setAlsoOpenElsewhere(bool alsoOpen);

  void setFocused(bool focused);

  bool isMaximized() const { return m_maximized; }

  void toggleMaximize();

  void setRestoreGeometry(const QRect &rect);

  QRect restoreGeometry() const { return m_restoreGeometry; }

  void setZOrder(int z) { m_zOrder = z; }
  int zOrder() const { return m_zOrder; }

  QSize preferredSize() const;

  // Show the conflict banner because the file on disk changed while
  // the in-memory document had unsaved edits.
  void showDiskConflictBanner();

  // Hide the conflict banner.
  void hideDiskConflictBanner();

  // Update the status pill to reflect that the in-memory document has
  // unsaved edits.
  void refreshModifiedIndicator();

  static constexpr int kMinimumWidth = 240;
  static constexpr int kMinimumHeight = 160;

signals:
  void closeRequested(WorkstationWindow *window);
  void focusRequested(WorkstationWindow *window);
  void geometryChanged(WorkstationWindow *window);

  void dragFinished(WorkstationWindow *window);
  void resizeFinished(WorkstationWindow *window);

  void closeAllRequested();
  void autoArrangeRequested();
  void tileRequested();
  void cascadeRequested();

  // Emitted when the user picks an action in the conflict banner.
  void diskConflictReloadRequested(WorkstationWindow *window);
  void diskConflictKeepMineRequested(WorkstationWindow *window);
  void diskConflictOverwriteRequested(WorkstationWindow *window);

protected:
  void mousePressEvent(QMouseEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void mouseDoubleClickEvent(QMouseEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  void changeEvent(QEvent *event) override;
  void contextMenuEvent(QContextMenuEvent *event) override;
  void keyPressEvent(QKeyEvent *event) override;

private:
  QWidget *buildHeader();
  QWidget *buildConflictBanner();

  ResizeEdge edgeAt(const QPoint &pos) const;
  void updateCursorForEdge(ResizeEdge edge);

  bool isDragHandlePoint(const QPoint &pos) const;

  void beginDrag(const QPoint &globalPos);
  void beginResize(const QPoint &globalPos, ResizeEdge edge);
  void applyDrag(const QPoint &globalPos);
  void applyResize(const QPoint &globalPos);

  void clampToParent();

  TextDocument *m_document = nullptr;
  QWidget *m_body = nullptr;
  QString m_filePath;

  QWidget *m_header = nullptr;
  QLabel *m_titleLabel = nullptr;
  QLabel *m_statusPill = nullptr;
  QLabel *m_alsoOpenBadge = nullptr;
  QPushButton *m_maximizeButton = nullptr;
  QPushButton *m_closeButton = nullptr;

  QWidget *m_conflictBanner = nullptr;

  bool m_maximized = false;
  QRect m_restoreGeometry;

  bool m_dragging = false;
  bool m_resizing = false;
  ResizeEdge m_resizeEdge = ResizeEdge::None;

  QPoint m_dragOffset;
  QRect m_resizeStartGeometry;
  QPoint m_resizeStartGlobal;

  int m_zOrder = 0;

  static constexpr int kHeaderHeight = 30;
  static constexpr int kResizeBorder = 6;
  static constexpr int kDragPaddingTop = 2;
};