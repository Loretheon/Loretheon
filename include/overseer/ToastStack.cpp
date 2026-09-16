#include "../../include/overseer/ToastStack.h"

#include "../../include/overseer/MemoryProposalToast.h"

#include <QPropertyAnimation>
#include <QVBoxLayout>
#include <qcoreevent.h>

ToastStack::ToastStack(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("overseerToastStack"));
  setAttribute(Qt::WA_TransparentForMouseEvents, true);
  setAttribute(Qt::WA_NoSystemBackground, true);
  setAttribute(Qt::WA_TranslucentBackground, true);
  setFocusPolicy(Qt::NoFocus);

  m_layout = new QVBoxLayout(this);
  m_layout->setContentsMargins(0, 0, 0, 0);
  m_layout->setSpacing(SpacingPx);
  m_layout->setAlignment(Qt::AlignTop | Qt::AlignRight);

  if (parent) {
    parent->installEventFilter(this);
  }
}


bool ToastStack::eventFilter(QObject *watched, QEvent *event) {
  if (watched == parentWidget() && event->type() == QEvent::Resize) {
    reposition();
  }
  return QWidget::eventFilter(watched, event);
}


void ToastStack::showProposalToast(const QString &fact,
                                   const QString &rationale) {
  // Cap: dismiss the oldest if we're at capacity.
  while (m_toasts.size() >= StackCap) {
    MemoryProposalToast *oldest = m_toasts.takeFirst();
    if (oldest) {
      oldest->dismiss();
    }
  }

  auto *toast = new MemoryProposalToast(fact, rationale, 8000, this);
  toast->setFixedWidth(ToastWidthPx);

  // Prepend: newest on top.
  m_layout->insertWidget(0, toast);
  m_toasts.append(toast);

  connect(toast, &MemoryProposalToast::finished, this, [this, toast]() {
    m_toasts.removeOne(toast);

    if (m_layout) {
      m_layout->removeWidget(toast);
    }

    toast->deleteLater();

    reposition();
  });

  // Entry animation: slide in from the right by animating the widget's
  // x offset via a geometry animation against the target position.
  toast->show();
  toast->adjustSize();
  toast->setFixedWidth(ToastWidthPx);
  toast->adjustSize();

  const QPoint target = toast->pos();

  QPropertyAnimation *slide = new QPropertyAnimation(toast, "pos", toast);
  slide->setDuration(1000);
  slide->setStartValue(QPoint(target.x() + ToastWidthPx, target.y()));
  slide->setEndValue(target);
  slide->setEasingCurve(QEasingCurve::OutCubic);
  slide->start(QAbstractAnimation::DeleteWhenStopped);

  reposition();
}

void ToastStack::dismissAll() {
  const QList<MemoryProposalToast *> snapshot = m_toasts;
  m_toasts.clear();

  for (MemoryProposalToast *toast : snapshot) {
    if (toast) {
      toast->dismiss();
    }
  }
}

void ToastStack::reposition() {
  if (!parentWidget()) {
    return;
  }

  const int parentWidth = parentWidget()->width();

  // Fixed width, but never wider than half the parent.
  const int width = qMin(ToastWidthPx, qMax(240, parentWidth / 2));

  for (MemoryProposalToast *toast : std::as_const(m_toasts)) {
    if (toast) {
      toast->setFixedWidth(width);
    }
  }

  // Let the layout size us; then place ourselves top-right.
  adjustSize();

  const int x = parentWidget()->width() - width - MarginPx;
  const int y = MarginPx;

  setGeometry(x, y, width, sizeHint().height());
  raise();
}