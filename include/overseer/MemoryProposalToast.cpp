#include "../../include/overseer/MemoryProposalToast.h"

#include <QGraphicsOpacityEffect>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPropertyAnimation>
#include <QTimer>
#include <QVBoxLayout>

namespace {
constexpr int SlideInMs = 1000;
constexpr int FadeOutMs = 250;
constexpr int ToastRadius = 8;
} // namespace

MemoryProposalToast::MemoryProposalToast(const QString &fact,
                                         const QString &rationale,
                                         int lifetimeMs, QWidget *parent)
    : QWidget(parent), m_lifetimeMs(lifetimeMs) {
  setObjectName(QStringLiteral("memoryProposalToast"));
  setAttribute(Qt::WA_TransparentForMouseEvents, true);
  setAttribute(Qt::WA_ShowWithoutActivating, true);

  m_factLabel = new QLabel(fact, this);
  m_factLabel->setWordWrap(true);
  m_factLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);

  {
    QFont bold = m_factLabel->font();
    bold.setBold(true);
    m_factLabel->setFont(bold);
  }

  m_rationaleLabel = new QLabel(rationale, this);
  m_rationaleLabel->setWordWrap(true);
  m_rationaleLabel->setAttribute(Qt::WA_TransparentForMouseEvents, true);
  m_rationaleLabel->setVisible(!rationale.isEmpty());

  {
    QFont small = m_rationaleLabel->font();
    small.setPointSizeF(qMax(7.0, small.pointSizeF() - 1.0));
    m_rationaleLabel->setFont(small);
  }

  auto *header = new QLabel(tr("Memory proposal"), this);
  header->setAttribute(Qt::WA_TransparentForMouseEvents, true);
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

  // Fade support.
  m_opacity = new QGraphicsOpacityEffect(this);
  m_opacity->setOpacity(1.0);
  setGraphicsEffect(m_opacity);

  m_fade = new QPropertyAnimation(m_opacity, "opacity", this);
  m_fade->setDuration(FadeOutMs);

  connect(m_fade, &QPropertyAnimation::finished, this, [this]() {
    emit finished();
  });

  m_lifetimeTimer = new QTimer(this);
  m_lifetimeTimer->setSingleShot(true);
  m_lifetimeTimer->setInterval(m_lifetimeMs);

  connect(m_lifetimeTimer, &QTimer::timeout, this,
          &MemoryProposalToast::startFadeOut);

  m_lifetimeTimer->start();
}

void MemoryProposalToast::dismiss() {
  if (m_dismissed) {
    return;
  }

  m_dismissed = true;

  if (m_lifetimeTimer) {
    m_lifetimeTimer->stop();
  }

  startFadeOut();
}

void MemoryProposalToast::startFadeOut() {
  if (m_dismissed && m_fade->state() == QPropertyAnimation::Running) {
    return;
  }

  m_dismissed = true;

  if (m_lifetimeTimer) {
    m_lifetimeTimer->stop();
  }

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
  event->ignore();
}

void MemoryProposalToast::mouseReleaseEvent(QMouseEvent *event) {
  event->ignore();
}