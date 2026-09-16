#pragma once

#include <QWidget>

class MemoryProposalToast;

class QVBoxLayout;

// A floating container anchored to the top-right of its parent. Owns a
// small stack of MemoryProposalToast widgets, caps the concurrent count,
// and repositions itself on parent resize.
//
// The stack does not interpret mouse events; the toasts themselves are
// non-interactive. This widget is purely a positioning/stacking helper.
class ToastStack : public QWidget {
  Q_OBJECT

public:
  explicit ToastStack(QWidget *parent);

  // Slide in a new toast. If the stack is at capacity, the oldest toast
  // is dismissed immediately to make room.
  void showProposalToast(const QString &fact, const QString &rationale);

  // Remove all live toasts. Called when the session changes.
  void dismissAll();

  // Called by the parent widget's resizeEvent.
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