#pragma once

#include <QWidget>

class QLabel;
class QPropertyAnimation;
class QTimer;

// A container that shows one child widget (the "content") and hides it
// automatically when the mouse leaves it, unless it is pinned open.
//
// Unlike an overlay dock, this dock is placed in its own layout slot.
// When hidden, the slot collapses to a small "strip" width. When shown,
// the slot expands to the persisted dock width. The dock never
// overlaps neighbouring content and never clips it.
//
// The dock does not manage the content widget's lifetime. Callers pass
// the content in the constructor and the dock takes ownership by
// reparenting it.
class AutoHideDock : public QWidget {
  Q_OBJECT

public:
  enum class Edge { Left, Right };

  AutoHideDock(Edge edge, QWidget *parent);

  // Set the widget that lives inside the dock. The dock takes ownership
  // and reparents it.
  void setContent(QWidget *content);

  QWidget *content() const { return m_content; }

  bool isPinned() const { return m_pinned; }

  void setPinned(bool pinned);
  void togglePinned();

  void showDock();
  void hideDock();

  bool isExpanded() const { return m_expanded; }

  int dockWidth() const { return m_dockWidth; }
  void setDockWidth(int width);

  // The strip width that stays visible when the dock is collapsed.
  static constexpr int kStripWidth = 6;

  static constexpr int kResizeHandleWidth = 4;

  static constexpr int kHideDelayMs = 400;
  static constexpr int kSlideDurationMs = 180;

  static constexpr int kMinDockWidth = 160;
  static constexpr int kMaxDockWidth = 720;

signals:
  void pinnedChanged(bool pinned);
  void dockWidthChanged(int width);
  void expandedChanged(bool expanded);

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;
  void enterEvent(QEnterEvent *event) override;
  void leaveEvent(QEvent *event) override;
  void resizeEvent(QResizeEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
  void mousePressEvent(QMouseEvent *event) override;
  void mouseReleaseEvent(QMouseEvent *event) override;

private:
  void scheduleHide();
  void cancelHide();

  bool mouseIsOverDock() const;
  bool mouseIsOverStrip() const;

  void updateStripGeometry();

  void updateCursor(const QPoint &pos);

  void loadPersistedState();
  void persistWidth();
  void persistExpanded();

  QString widthSettingsKey() const;
  QString expandedSettingsKey() const;

  Edge m_edge;
  QWidget *m_content = nullptr;

  bool m_pinned = false;
  bool m_expanded = true;

  int m_dockWidth = 280;

  bool m_resizing = false;
  int m_resizeStartWidth = 0;
  int m_resizeStartGlobalX = 0;

  QTimer *m_hideTimer = nullptr;

  // The strip: a small widget pinned to the outer edge of the dock,
  // visible only when the dock is collapsed. Its geometry is managed
  // by this class, not by a layout.
  QWidget *m_strip = nullptr;
  QLabel *m_stripChevron = nullptr;
};