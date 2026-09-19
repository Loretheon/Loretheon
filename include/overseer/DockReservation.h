#pragma once

#include <QWidget>

class AutoHideDock;
class QPropertyAnimation;

// A wrapper placed in the Overseer page's layout. Its only job is to
// have a fixed width that animates between the strip width (dock
// hidden) and the dock width (dock shown). The dock content is a child
// of the wrapper and fills it entirely.
//
// This is what actually reserves space in the layout: the center
// column never overlaps with the dock, because the dock is a real
// layout sibling.
class DockReservation : public QWidget {
  Q_OBJECT

public:
  explicit DockReservation(AutoHideDock *dock, QWidget *parent);

  AutoHideDock *dock() const { return m_dock; }

  // Animate to a target width. Called internally when the dock's
  // expanded state or dock width changes.
  void animateToWidth(int width, bool animated);

  // Sync from the dock's current state. Called on construction and on
  // dock state changes.
  void syncToDockState(bool animated);

protected:
  void resizeEvent(QResizeEvent *event) override;

private:
  AutoHideDock *m_dock = nullptr;
  QPropertyAnimation *m_animation = nullptr;
  int m_currentWidth = 0;
};