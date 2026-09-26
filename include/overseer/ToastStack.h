#pragma once

#include "NotificationService.h"

#include <QWidget>

class NotificationToast;

class QVBoxLayout;

// The visual host for toasts. NotificationService drives it; nothing
// else should call into it. The stack owns a small queue of live toasts
// and repositions itself on parent resize.
//
// In production this is parented to the top-level Lore window, not to
// any page inside it, so toasts overlay the whole application.
class ToastStack : public QWidget {
  Q_OBJECT

public:
  explicit ToastStack(QWidget *parent);

  // Show a toast for the given notification. Called by
  // NotificationService. Lifetime of 0 means the toast does not expire
  // on its own and must be dismissed by the user.
  void showNotification(const QString &id, const QString &title,
                        const QString &body,
                        NotificationService::Severity severity,
                        int lifetimeMs);

  void dismissAll();

  void reposition();

protected:
  bool eventFilter(QObject *watched, QEvent *event) override;

private:
  static constexpr int StackCap = 5;
  static constexpr int MarginPx = 16;
  static constexpr int SpacingPx = 10;
  static constexpr int ToastWidthPx = 400;

  QVBoxLayout *m_layout = nullptr;
  QList<NotificationToast *> m_toasts;
};