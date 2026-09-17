#include "../../include/overseer/MemoryProposalToast.h"

#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPropertyAnimation>
#include <QTimer>
#include <QVBoxLayout>

namespace {
constexpr int FadeOutMs = 250;
constexpr int ToastRadius = 8;
} // namespace

MemoryProposalToast::MemoryProposalToast(const QString &fact,
                                         const QString &rationale,
                                         const QString &scopeLabel,
                                         int lifetimeMs, QWidget *parent)
    : QWidget(parent), m_lifetimeMs(lifetimeMs) {
  setObjectName(QStringLiteral("memoryProposalToast"));
  setAttribute(Qt::WA_ShowWithoutActivating, true);
  setAttribute(Qt::WA_StyledBackground, true);
  setCursor(Qt::PointingHandCursor);
  setToolTip(tr("Click to dismiss, or drag aside."));

  setAutoFillBackground(false);

  m_factLabel = new QLabel(fact, this);
  m_factLabel->setWordWrap(true);

  {
    QFont bold = m_factLabel->font();
    bold.setBold(true);
    m_factLabel->setFont(bold);
  }

  m_rationaleLabel = new QLabel(rationale, this);
  m_rationaleLabel->setWordWrap(true);
  m_rationaleLabel->setVisible(!rationale.isEmpty());

  {
    QFont small = m_rationaleLabel->font();
    small.setPointSizeF(qMax(7.0, small.pointSizeF() - 1.0));
    m_rationaleLabel->setFont(small);
  }

  const QString headerText =
      scopeLabel.isEmpty()
          ? tr("Memory proposal")
          : tr("Memory proposal · %1").arg(scopeLabel);

  auto *header = new QLabel(headerText, this);
  {
    QFont small = header->font();
    small.setPointSizeF(qMax(7.0, small.pointSizeF() - 1.0));
    header->setFont(small);
  }

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(12, 10, 12, 10);
  layout->setSpacing(4);
  layout->addWidget(header);
  layout->addWidget(m_factLabel);
  layout->addWidget(m_rationaleLabel);

  m_opacity = new QGraphicsOpacityEffect(this);
  m_opacity->setOpacity(1.0);

  m_fade = new QPropertyAnimation(m_opacity, "opacity", this);
  m_fade->setDuration(FadeOutMs);

  connect(m_fade, &QPropertyAnimation::finished, this,
          [this]() { emit finished(); });

  m_lifetimeTimer = new QTimer(this);
  m_lifetimeTimer->setSingleShot(true);
  m_lifetimeTimer->setInterval(m_lifetimeMs);

  connect(m_lifetimeTimer, &QTimer::timeout, this,
          &MemoryProposalToast::startFadeOut);

  m_lifetimeTimer->start();
}

void MemoryProposalToast::dismiss() {
  if (m_dismissed)
    return;

  m_dismissed = true;

  if (m_lifetimeTimer)
    m_lifetimeTimer->stop();

  startFadeOut();
}

void MemoryProposalToast::startFadeOut() {
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

void MemoryProposalToast::paintEvent(QPaintEvent *event) {
  Q_UNUSED(event);

  QPainter painter(this);
  painter.setRenderHint(QPainter::Antialiasing, true);

  QColor bg = palette().color(QPalette::Window);
  bg.setAlpha(240);

  QColor border = palette().color(QPalette::Mid);
  border.setAlpha(120);

  painter.setBrush(bg);
  painter.setPen(border);

  const QRectF r = rect().adjusted(0.5, 0.5, -0.5, -0.5);
  painter.drawRoundedRect(r, ToastRadius, ToastRadius);
}

void MemoryProposalToast::mousePressEvent(QMouseEvent *event) {
  if (event->button() != Qt::LeftButton) {
    QWidget::mousePressEvent(event);
    return;
  }

  m_dragging = true;
  m_dragStartGlobal = event->globalPosition().toPoint();

  event->accept();
}

void MemoryProposalToast::mouseMoveEvent(QMouseEvent *event) {
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

void MemoryProposalToast::mouseReleaseEvent(QMouseEvent *event) {
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