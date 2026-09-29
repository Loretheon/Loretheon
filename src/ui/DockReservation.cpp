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

    if (m_dock->orientation() == Qt::Horizontal)
      m_dock->setFixedWidth(AutoHideDock::StripWidth);
    else
      m_dock->setFixedHeight(AutoHideDock::StripWidth);
  }

  m_animation = new QPropertyAnimation(this, "reservedLength", this);
  m_animation->setDuration(AutoHideDock::SlideDurationMs);
  m_animation->setEasingCurve(QEasingCurve::InOutCubic);

  if (m_dock) {
    connect(m_dock, &AutoHideDock::expandedChanged, this,
            [this](bool) { syncToDockState(true); });

    connect(m_dock, &AutoHideDock::dockLengthChanged, this,
            [this](int) { syncToDockState(true); });
  }

  syncToDockState(false);
}

void DockReservation::applyLength(int length) {
  const int target = std::max(AutoHideDock::StripWidth, length);

  m_currentLength = target;

  if (!m_dock) {
    setFixedWidth(target);
    return;
  }

  if (m_dock->orientation() == Qt::Horizontal) {
    setFixedWidth(target);
    setMinimumHeight(0);
    setMaximumHeight(QWIDGETSIZE_MAX);
    m_dock->setFixedWidth(target);
  } else {
    setFixedHeight(target);
    setMinimumWidth(0);
    setMaximumWidth(QWIDGETSIZE_MAX);
    m_dock->setFixedHeight(target);
  }
}

void DockReservation::setReservedLength(int length) {
  const int target = std::max(AutoHideDock::StripWidth, length);

  if (m_currentLength == target)
    return;

  applyLength(target);
}

void DockReservation::animateToLength(int length, bool animated) {
  const int target = std::max(AutoHideDock::StripWidth, length);

  if (m_currentLength == target)
    return;

  if (animated) {
    m_animation->stop();
    m_animation->setStartValue(m_currentLength);
    m_animation->setEndValue(target);
    m_animation->start();
  } else {
    m_animation->stop();
    setReservedLength(target);
  }
}

void DockReservation::syncToDockState(bool animated) {
  if (!m_dock)
    return;

  const int target =
      m_dock->isExpanded()
          ? std::max(AutoHideDock::MinDockLength, m_dock->dockLength())
          : AutoHideDock::StripWidth;

  animateToLength(target, animated);
}

void DockReservation::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);

  if (m_dock)
    m_dock->setGeometry(rect());
}