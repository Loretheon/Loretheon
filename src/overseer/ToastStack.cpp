#include "../../include/overseer/ToastStack.h"

#include "../../include/app/Settings.h"
#include "../../include/overseer/MemoryProposalToast.h"

#include <QEvent>
#include <QPropertyAnimation>
#include <QTimer>
#include <QVBoxLayout>

ToastStack::ToastStack(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("overseerToastStack"));
  setAttribute(Qt::WA_TransparentForMouseEvents, true);
  setAttribute(Qt::WA_ShowWithoutActivating, true);
  setFocusPolicy(Qt::NoFocus);

  // No WA_TranslucentBackground, no WA_NoSystemBackground. Those fight
  // with child QGraphicsOpacityEffect on X11 and cause the toasts to
  // render nothing.

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
  while (m_toasts.size() >= StackCap) {
    MemoryProposalToast *oldest = m_toasts.takeFirst();
    if (oldest) {
      oldest->dismiss();
    }
  }

  const int lifetime = Settings::getOverseerToastDurationMs();

  auto *toast = new MemoryProposalToast(fact, rationale, lifetime, this);

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

  // Defer positioning until the widget has been laid out at least once.
  QTimer::singleShot(0, this, [this, toast]() {
    reposition();

    if (!toast) {
      return;
    }

    // Target position is where the layout has placed it.
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

  if (parentWidth <= 0) {
    return;
  }

  const int width = qMin(ToastWidthPx, qMax(240, parentWidth / 2));

  for (MemoryProposalToast *toast : std::as_const(m_toasts)) {
    if (toast) {
      toast->setFixedWidth(width);
    }
  }

  // Let the layout compute our size, then anchor top-right.
  adjustSize();

  const int x = parentWidth - width - MarginPx;
  const int y = MarginPx;

  setGeometry(x, y, width, qMax(1, sizeHint().height()));

  raise();
}