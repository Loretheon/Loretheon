#include "../../include/overseer/DockReservation.h"

#include "AutoHideDock.h"

#include <QPropertyAnimation>
#include <QResizeEvent>

DockReservation::DockReservation(AutoHideDock *dock, QWidget *parent)
    : QWidget(parent), m_dock(dock) {
  setObjectName(QStringLiteral("dockReservation"));

  if (m_dock) {
    m_dock->setParent(this);
    m_dock->setFixedWidth(AutoHideDock::kStripWidth);
  }

  m_animation = new QPropertyAnimation(this, "minimumWidth", this);
  m_animation->setDuration(AutoHideDock::kSlideDurationMs);
  m_animation->setEasingCurve(QEasingCurve::InOutCubic);

  connect(m_animation, &QPropertyAnimation::valueChanged, this,
          [this](const QVariant &value) {
            const int w = value.toInt();

            m_currentWidth = w;

            setFixedWidth(w);

            if (m_dock)
              m_dock->setFixedWidth(w);
          });

  connect(m_animation, &QPropertyAnimation::finished, this, [this]() {
    const int w = m_animation->endValue().toInt();

    m_currentWidth = w;

    setFixedWidth(w);

    if (m_dock)
      m_dock->setFixedWidth(w);
  });

  if (m_dock) {
    connect(m_dock, &AutoHideDock::expandedChanged, this,
            [this](bool) { syncToDockState(true); });

    connect(m_dock, &AutoHideDock::dockWidthChanged, this,
            [this](int) { syncToDockState(true); });
  }

  syncToDockState(false);
}

void DockReservation::animateToWidth(int width, bool animated) {
  const int target = qMax(AutoHideDock::kStripWidth, width);

  if (m_currentWidth == target)
    return;

  if (animated) {
    m_animation->stop();
    m_animation->setStartValue(m_currentWidth);
    m_animation->setEndValue(target);
    m_animation->start();
  } else {
    m_animation->stop();

    m_currentWidth = target;

    setFixedWidth(target);

    if (m_dock)
      m_dock->setFixedWidth(target);
  }
}

void DockReservation::syncToDockState(bool animated) {
  if (!m_dock)
    return;

  const int target = m_dock->isExpanded()
                         ? qMax(AutoHideDock::kMinDockWidth, m_dock->dockWidth())
                         : AutoHideDock::kStripWidth;

  animateToWidth(target, animated);
}

void DockReservation::resizeEvent(QResizeEvent *event) {
  QWidget::resizeEvent(event);

  if (m_dock)
    m_dock->setGeometry(rect());
}