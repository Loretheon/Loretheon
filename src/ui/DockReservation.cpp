#include "DockReservation.h"

#include "AutoHideDock.h"

#include <QPropertyAnimation>
#include <QResizeEvent>

#include <algorithm>

DockReservation::DockReservation(AutoHideDock *dock, QWidget *parent)
    : QWidget(parent), m_dock(dock) {
  setObjectName(QStringLiteral("dockReservation"));

  if (m_dock) {
    m_dock->setParent(this);
    m_dock->setFixedWidth(AutoHideDock::StripWidth);
  }

  m_animation = new QPropertyAnimation(this, "reservedWidth", this);
  m_animation->setDuration(AutoHideDock::SlideDurationMs);
  m_animation->setEasingCurve(QEasingCurve::InOutCubic);

  if (m_dock) {
    connect(m_dock, &AutoHideDock::expandedChanged, this,
            [this](bool) { syncToDockState(true); });

    connect(m_dock, &AutoHideDock::dockWidthChanged, this,
            [this](int) { syncToDockState(true); });
  }

  syncToDockState(false);
}

void DockReservation::setReservedWidth(int width) {
  const int target = std::max(AutoHideDock::StripWidth, width);

  if (m_currentWidth == target)
    return;

  m_currentWidth = target;

  setFixedWidth(target);

  if (m_dock)
    m_dock->setFixedWidth(target);
}

void DockReservation::animateToWidth(int width, bool animated) {
  const int target = std::max(AutoHideDock::StripWidth, width);

  if (m_currentWidth == target)
    return;

  if (animated) {
    m_animation->stop();
    m_animation->setStartValue(m_currentWidth);
    m_animation->setEndValue(target);
    m_animation->start();
  } else {
    m_animation->stop();
    setReservedWidth(target);
  }
}

void DockReservation::syncToDockState(bool animated) {
  if (!m_dock)
    return;

  const int target =
      m_dock->isExpanded()
          ? std::max(AutoHideDock::MinDockWidth, m_dock->dockWidth())
          : AutoHideDock::StripWidth;

  animateToWidth(target, animated);
}

void DockReservation::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);

  if (m_dock)
    m_dock->setGeometry(rect());
}