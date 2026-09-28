#pragma once

#include <QWidget>

class AutoHideDock;
class QPropertyAnimation;

// A wrapper placed in a page's layout. Its only job is to have a fixed
// width that animates between the strip width (dock hidden) and the
// dock width (dock shown). The dock content is a child of the wrapper
// and fills it entirely.
//
// The wrapper exposes a proper int Q_PROPERTY for animation, rather
// than animating minimumWidth, so that the layout, the fixed-width
// state, and the animation all agree.
class DockReservation : public QWidget {
  Q_OBJECT

  Q_PROPERTY(int reservedWidth READ reservedWidth WRITE setReservedWidth)

public:
  explicit DockReservation(AutoHideDock *dock, QWidget *parent);

  AutoHideDock *dock() const { return m_dock; }

  int reservedWidth() const { return m_currentWidth; }
  void setReservedWidth(int width);

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