#include "../../include/app/NotificationToast.h"

#include "../../include/app/theme/ThemeTokens.h"
#include "ThemeRegistry.h"

#include <QEnterEvent>
#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPropertyAnimation>
#include <QTimer>
#include <QVBoxLayout>

NotificationToast::NotificationToast(const QString &id, const QString &title,
                                     const QString &body,
                                     NotificationService::Severity severity,
                                     int lifetimeMs, QWidget *parent)
    : QWidget(parent), m_id(id), m_severity(severity),
      m_lifetimeMs(lifetimeMs) {
  setObjectName(QStringLiteral("notificationToast"));
  setAttribute(Qt::WA_ShowWithoutActivating, true);
  setAttribute(Qt::WA_StyledBackground, true);
  setCursor(Qt::PointingHandCursor);
  setToolTip(tr("Click to dismiss, or drag aside."));

  setAutoFillBackground(false);

  m_titleLabel = new QLabel(title, this);
  m_titleLabel->setWordWrap(true);
  m_titleLabel->setObjectName(QStringLiteral("notificationToastTitle"));

  {
    QFont bold = m_titleLabel->font();
    bold.setBold(true);
    m_titleLabel->setFont(bold);
  }

  m_bodyLabel = new QLabel(body, this);
  m_bodyLabel->setWordWrap(true);
  m_bodyLabel->setObjectName(QStringLiteral("notificationToastBody"));
  m_bodyLabel->setVisible(!body.isEmpty());

  {
    QFont small = m_bodyLabel->font();
    small.setPointSizeF(qMax(7.0, small.pointSizeF() - 1.0));
    m_bodyLabel->setFont(small);
  }

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(14, 10, 14, 12);
  layout->setSpacing(4);
  layout->addWidget(m_titleLabel);
  layout->addWidget(m_bodyLabel);

  m_opacity = new QGraphicsOpacityEffect(this);
  m_opacity->setOpacity(1.0);

  m_fade = new QPropertyAnimation(m_opacity, "opacity", this);
  m_fade->setDuration(kFadeOutMs);

  connect(m_fade, &QPropertyAnimation::finished, this,
          [this]() { emit finished(); });

  if (m_lifetimeMs > 0) {
    m_lifetimeTimer = new QTimer(this);
    m_lifetimeTimer->setSingleShot(true);
    m_lifetimeTimer->setInterval(m_lifetimeMs);

    connect(m_lifetimeTimer, &QTimer::timeout, this,
            &NotificationToast::startFadeOut);

    m_lifetimeTimer->start();
  }

  refreshPalette();
}

void NotificationToast::refreshPalette() {
  const QString role = severityColorRole();

  const QColor accent =
      ThemeRegistry::instance().color(QStringLiteral("notification.%1")
                                          .arg(role));

  if (m_titleLabel) {
    m_titleLabel->setStyleSheet(
        QStringLiteral("color: %1;").arg(accent.name()));
  }

  update();
}

QString NotificationToast::severityColorRole() const {
  switch (m_severity) {
  case NotificationService::Severity::Info:
    return QStringLiteral("info");
  case NotificationService::Severity::Warning:
    return QStringLiteral("warning");
  case NotificationService::Severity::Error:
    return QStringLiteral("error");
  case NotificationService::Severity::Critical:
    return QStringLiteral("critical");
  }

  return QStringLiteral("info");
}

void NotificationToast::dismiss() {
  if (m_dismissed)
    return;

  m_dismissed = true;

  if (m_lifetimeTimer)
    m_lifetimeTimer->stop();

  startFadeOut();
}

void NotificationToast::startFadeOut() {
  if (m_dismissed && m_fade->state() == QPropertyAnimation::Running)
    return;

  m_dismissed = true;

  if (m_lifetimeTimer)
    m_lifetimeTimer->stop();

  setGraphicsEffect(m_opacity);

  m_fade->stop();
  m_fade->setStartValue(m_opacity->opacity());
  m_fade->setEndValue(0.0);
  m_fade->start();
}

void NotificationToast::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);

  const ThemeTokens tokens =
      ThemeRegistry::instance().tokens(ThemeRegistry::instance().activeTheme());

  QColor bg = tokens.base;
  bg.setAlpha(m_hovered ? 250 : 240);

  const QString role = severityColorRole();

  QColor border =
      ThemeRegistry::instance().color(QStringLiteral("notification.%1")
                                          .arg(role));
  border.setAlpha(m_hovered ? 220 : 160);

  painter.setBrush(bg);
  painter.setPen(QPen(border, m_hovered ? 1.6 : 1.2));

  const QRectF r = rect().adjusted(0.5, 0.5, -0.5, -0.5);
  painter.drawRoundedRect(r, kToastRadius, kToastRadius);

  // A thin severity stripe on the leading edge.
  QColor stripe = border;
  stripe.setAlpha(240);

  painter.setPen(Qt::NoPen);
  painter.setBrush(stripe);
  painter.drawRoundedRect(QRectF(0.5, 0.5, 4.0, height() - 1.0), 2.0, 2.0);
}

void NotificationToast::mousePressEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton) {
    QWidget::mousePressEvent(event);
    return;
  }

  m_dragging = true;
  m_dragStartGlobal = event->globalPosition().toPoint();

  event->accept();
}

void NotificationToast::mouseMoveEvent(QMouseEvent *event) {
  if (!m_dragging) {
    QWidget::mouseMoveEvent(event);
    return;
  }

  const QPoint delta =
      event->globalPosition().toPoint() - m_dragStartGlobal;

  if (delta.manhattanLength() > kDragDismissPx) {
    dismiss();
    m_dragging = false;
  }

  event->accept();
}

void NotificationToast::mouseReleaseEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton) {
    QWidget::mouseReleaseEvent(event);
    return;
  }

  const bool wasDragging = m_dragging;
  m_dragging = false;

  if (wasDragging)
    dismiss();

  event->accept();
}

void NotificationToast::enterEvent(QEnterEvent *event) {
  m_hovered = true;
  update();
  QWidget::enterEvent(event);
}

void NotificationToast::leaveEvent(QEvent *event) {
  m_hovered = false;
  update();
  QWidget::leaveEvent(event);
}