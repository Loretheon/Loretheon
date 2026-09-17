#include "../../include/overseer/ToastStack.h"

#include "../../include/app/Settings.h"
#include "../../include/overseer/MemoryProposalToast.h"

#include <QEvent>
#include <QPropertyAnimation>
#include <QTimer>
#include <QVBoxLayout>

ToastStack::ToastStack(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("overseerToastStack"));
  setAttribute(Qt::WA_ShowWithoutActivating, true);
  setFocusPolicy(Qt::NoFocus);
  // The stack does not draw; only the toasts do.
  setAttribute(Qt::WA_TransparentForMouseEvents, false);

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
                                   const QString &rationale,
                                   const QString &scopeLabel) {
  while (m_toasts.size() >= StackCap) {
    MemoryProposalToast *oldest = m_toasts.takeFirst();
    if (oldest) {
      oldest->dismiss();
    }
  }

  const int lifetime = Settings::getOverseerToastDurationMs();

  auto *toast =
      new MemoryProposalToast(fact, rationale, scopeLabel, lifetime, this);

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

  toast->show();

  QTimer::singleShot(0, this, [this, toast]() {
    reposition();

    if (!toast) {
      return;
    }

    const QPoint target = toast->pos();

    auto *slide = new QPropertyAnimation(toast, "pos", toast);
    slide->setDuration(1000);
    slide->setStartValue(QPoint(target.x() + toast->width(), target.y()));
    slide->setEndValue(target);
    slide->setEasingCurve(QEasingCurve::OutCubic);
    slide->start(QAbstractAnimation::DeleteWhenStopped);
  });

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
  QWidget *parent = parentWidget();

  if (!parent) {
    return;
  }

  const int parentWidth = parent->width();
  const int parentHeight = parent->height();

  if (parentWidth <= 0 || parentHeight <= 0) {
    return;
  }

  const int width = qMin(ToastWidthPx, qMax(240, parentWidth / 2));

  for (MemoryProposalToast *toast : std::as_const(m_toasts)) {
    if (toast) {
      toast->setFixedWidth(width);
    }
  }

  adjustSize();

  const int x = qMax(MarginPx, parentWidth - width - MarginPx);
  const int y = MarginPx;

  setGeometry(x, y, width, qMax(1, sizeHint().height()));

  raise();
}