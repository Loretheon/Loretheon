#pragma once

#include <QString>
#include <QWidget>

class QTimer;

class AutoHideDock : public QWidget {
  Q_OBJECT

public:
  enum class Edge { Left, Right };

  static constexpr int StripWidth = 10;
  static constexpr int MinDockWidth = 180;
  static constexpr int SlideDurationMs = 90;

  // Width of the drag handle in pixels. Must be at least as wide as the
  // widest visual affordance (the grip bar drawn inside it), and wide
  // enough to comfortably grab with a mouse. The grip is drawn centred
  // in this band by the stylesheet.
  static constexpr int ResizeHandleWidth = 8;

  explicit AutoHideDock(Edge edge, const QString &settingsPrefix,
                        QWidget *parent = nullptr);

  void setContent(QWidget *content);
  QWidget *content() const { return m_content; }

  void setDockWidth(int width);
  int dockWidth() const { return m_dockWidth; }

  void setMaxDockWidth(int width);
  int maxDockWidth() const { return m_maxDockWidth; }

  // Width the content would like to be. Normally the content's own
  // sizeHint().width(), or a value it computed from its children.
  void setPreferredContentWidth(int width);
  int preferredContentWidth() const { return m_preferredContentWidth; }

  // Resize to the preferred content width, clamped to
  // [MinDockWidth, m_maxDockWidth]. No-op if unchanged.
  void fitToContentWidth();

  // True once the user has dragged the resize handle. Once true, the
  // dock will not auto-fit to content any more.
  bool hasUserOverrideWidth() const { return m_userOverrideWidth; }

  // Forget the user override. Called when the content's identity or
  // context changes and the dock should be allowed to fit again.
  void clearUserOverrideWidth();

  void setPinned(bool pinned);
  bool isPinned() const { return m_pinned; }
  void togglePinned();

  bool isExpanded() const { return m_expanded; }
  void showDock();
  void hideDock();

  // Immediately expand or collapse, without animating. Used by the
  // hosting layout when the dock must be at its final size before the
  // next paint.
  void showDockImmediate();
  void hideDockImmediate();

  void scheduleHide();
  void cancelHide();

  // Shorten the auto-hide delay. The default is 400 ms; "extremely
  // responsive" mode uses a much shorter window so the dock gets out
  // of the way as soon as the mouse leaves.
  void setHideDelayMs(int ms);
  int hideDelayMs() const;

signals:
  void dockWidthChanged(int width);
  void expandedChanged(bool expanded);
  void pinnedChanged(bool pinned);

protected:
  void enterEvent(QEnterEvent *event) override;
  void leaveEvent(QEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;
  void paintEvent(QPaintEvent *event) override;
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  QString widthSettingsKey() const;
  QString overrideSettingsKey() const;
  QString expandedSettingsKey() const;

  void loadPersistedState();
  void persistWidth();
  void persistOverride();
  void persistExpanded();

  bool mouseIsOverDock() const;
  bool mouseIsOverStrip() const;

  // Whether the mouse is over the drag handle band. Computed against
  // the dock's own rect, regardless of whether the dock is expanded.
  bool mouseIsOverResizeHandle(const QPoint &localPos) const;

  void updateCursor(const QPoint &pos);
  void updateStripGeometry();
  void updateContentGeometry();

  void recomputeMaxWidthFromScreen();

  static constexpr int kDefaultHideDelayMs = 1000;

  Edge m_edge;
  QString m_settingsPrefix;

  QWidget *m_content = nullptr;
  QWidget *m_strip = nullptr;
  QWidget *m_stripChevron = nullptr;

  QTimer *m_hideTimer = nullptr;

  int m_dockWidth = 280;
  int m_maxDockWidth = 1200;
  int m_preferredContentWidth = 0;
  bool m_expanded = true;
  bool m_pinned = false;
  bool m_userOverrideWidth = false;

  bool m_resizing = false;
  int m_resizeStartWidth = 0;
  int m_resizeStartGlobalX = 0;

  // Set while the cursor is inside the resize-handle band, so the
  // paintEvent can draw the grip even when the dock is expanded.
  bool m_hoveringHandle = false;
};