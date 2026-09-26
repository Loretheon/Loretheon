#pragma once

#include <QPoint>
#include <QRect>
#include <QString>
#include <QTimer>
#include <QWidget>

class QLabel;
class QPushButton;
class TextDocument;
class EditSession;

class WorkstationWindow : public QWidget {
  Q_OBJECT

public:
  void setLocked(bool locked);
  bool isLocked() const { return m_locked; }
  enum class Mode {
    Tiled,
    Floating,
  };

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

  enum class Status {
    Neutral,
    Modified,
    Opening,
    Rewriting,
    Review,
    Applied,
    Failed,
    ExternalChange,
    Conflict,
    Saved,
  };

  WorkstationWindow(TextDocument *document, QWidget *body,
                    const QString &filePath, QWidget *parent);

  ~WorkstationWindow() override;

  TextDocument *document() const { return m_document; }
  QWidget *body() const { return m_body; }
  QString filePath() const { return m_filePath; }
  QString displayName() const;

  void setEditSession(EditSession *session);
  EditSession *editSession() const { return m_editSession; }

  Status status() const { return m_status; }

  void setStatus(Status status, const QString &text = QString());

  void setTransientStatus(Status status, const QString &text,
                          int durationMs = 60000);

  void refreshNeutralStatus();

  void setAlsoOpenElsewhere(bool alsoOpen);
  void setFocused(bool focused);
  void setDragOverHighlight(bool highlighted);
  void setDropTargetHighlight(bool highlighted);

  Mode mode() const { return m_mode; }
  void setMode(Mode mode);

  bool isMaximized() const { return m_maximized; }
  void toggleMaximize();
  void setRestoreGeometry(const QRect &rect);
  QRect restoreGeometry() const { return m_restoreGeometry; }

  void setZOrder(int z) { m_zOrder = z; }
  int zOrder() const { return m_zOrder; }

  QSize preferredSize() const;

  void showDiskConflictBanner();
  void hideDiskConflictBanner();

  static constexpr int kMinimumWidth = 240;
  static constexpr int kMinimumHeight = 160;

signals:
  void closeRequested(WorkstationWindow *window);
  void focusRequested(WorkstationWindow *window);
  void geometryChanged(WorkstationWindow *window);

  void dragStarted(WorkstationWindow *window);
  void dragMoved(WorkstationWindow *window, const QPoint &globalPos);
  void dragFinished(WorkstationWindow *window, const QPoint &globalPos);
  void resizeFinished(WorkstationWindow *window);

  void closeAllRequested();
  void tileAllRequested();
  void floatAllRequested();
  void autoArrangeRequested();

  void diskConflictReloadRequested(WorkstationWindow *window);
  void diskConflictKeepMineRequested(WorkstationWindow *window);
  void diskConflictOverwriteRequested(WorkstationWindow *window);

  void rewriteRequested(WorkstationWindow *window);
  void modeChangeRequested(WorkstationWindow *window, Mode mode);

  void statusChanged(const QString &filePath, Status status);

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;
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

  void installFocusFilter(QWidget *w);

  ResizeEdge edgeAt(const QPoint &pos) const;
  void updateCursorForEdge(ResizeEdge edge);

  bool isDragHandlePoint(const QPoint &pos) const;

  void beginDrag(const QPoint &globalPos);
  void beginResize(const QPoint &globalPos, ResizeEdge edge);
  void applyDrag(const QPoint &globalPos);
  void applyResize(const QPoint &globalPos);

  void applyStatusVisuals();

  TextDocument *m_document = nullptr;
  QWidget *m_body = nullptr;
  QString m_filePath;

  EditSession *m_editSession = nullptr;

  QWidget *m_header = nullptr;
  QLabel *m_titleLabel = nullptr;
  QLabel *m_statusPill = nullptr;
  QPushButton *m_closeButton = nullptr;

  QWidget *m_conflictBanner = nullptr;

  Mode m_mode = Mode::Tiled;
  Status m_status = Status::Neutral;

  bool m_maximized = false;
  QRect m_restoreGeometry;

  bool m_dragging = false;
  bool m_resizing = false;
  ResizeEdge m_resizeEdge = ResizeEdge::None;

  QPoint m_dragOffset;
  QPoint m_dragGlobalStart;
  QRect m_resizeStartGeometry;
  QPoint m_resizeStartGlobal;

  int m_zOrder = 0;

  bool m_dragOverHighlight = false;
  bool m_dropTargetHighlight = false;
  bool m_alsoOpenElsewhere = false;
  bool m_locked = false;
  QTimer *m_transientTimer = nullptr;

  static constexpr int kHeaderHeight = 30;
  static constexpr int kResizeBorder = 6;
  static constexpr int kDragPaddingTop = 2;
};