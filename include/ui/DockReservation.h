#pragma once

#include <QWidget>

class AutoHideDock;
class QPropertyAnimation;

// A wrapper placed in a page's layout. Its only job is to have a fixed
// length along the dock's axis that animates between the strip length
// (dock hidden) and the dock length (dock shown). The dock content is a
// child of the wrapper and fills it entirely.
//
// The wrapper exposes a proper int Q_PROPERTY for animation, rather
// than animating minimumWidth/minimumHeight, so that the layout, the
// fixed-length state, and the animation all agree.
//
// For a horizontal dock (Left or Right), "length" is the reserved
// width and the wrapper's size policy is fixed-width. For a vertical
// dock (Top or Bottom), it is the reserved height and the policy is
// fixed-height. The wrapper is orientation-agnostic from the caller's
// perspective.
class DockReservation : public QWidget {
  Q_OBJECT

  Q_PROPERTY(int reservedLength READ reservedLength WRITE setReservedLength)

public:
  explicit DockReservation(AutoHideDock *dock, QWidget *parent);

  AutoHideDock *dock() const { return m_dock; }

  int reservedLength() const { return m_currentLength; }
  void setReservedLength(int length);

  // Animate to a target length. Called internally when the dock's
  // expanded state or dock length changes.
  void animateToLength(int length, bool animated);

  // Sync from the dock's current state. Called on construction and on
  // dock state changes.
  void syncToDockState(bool animated);

protected:
  void resizeEvent(QResizeEvent *event) override;

private:
  void applyLength(int length);

  AutoHideDock *m_dock = nullptr;
  QPropertyAnimation *m_animation = nullptr;
  int m_currentLength = 0;
};