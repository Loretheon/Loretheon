#pragma once

#include <QWidget>

class MemoryProposalToast;

class QVBoxLayout;

// A floating container anchored to the top-right of its parent. Owns a
// small stack of MemoryProposalToast widgets, caps the concurrent count,
// and repositions itself on parent resize.
//
// In production this is parented to the OverseerPage, not to the
// OverseerWidget, so the toasts sit over the whole page rather than over
// the short input strip. The stack itself does not accept mouse events;
// individual toasts do, so they can be clicked or dragged away.
class ToastStack : public QWidget {
  Q_OBJECT

public:
  explicit ToastStack(QWidget *parent);

  void showProposalToast(const QString &fact, const QString &rationale,
                         const QString &scopeLabel);

  void dismissAll();

  void reposition();

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  static constexpr int StackCap = 3;
  static constexpr int MarginPx = 12;
  static constexpr int SpacingPx = 8;
  static constexpr int ToastWidthPx = 360;

  QVBoxLayout *m_layout = nullptr;
  QList<MemoryProposalToast *> m_toasts;
};