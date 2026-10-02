#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QVector>

class ToastStack;

// The single notification hub for the application. Every user-facing
// message that the app needs to surface goes through here. Nothing else
// calls ToastStack directly, and nothing else fires a QSystemTrayIcon
// notification.
//
// Severity tiers:
//
//   Info           — a fact the user may want. Transient in-app toast.
//                    No OS notification, ever. Expires and is not kept.
//   Warning        — something needs attention but is not blocking.
//                    In-app toast with longer lifetime. Retained in
//                    the top-window "Recent" list.
//   Error          — something failed. Same retention as Warning, plus
//                    the top window shows a count badge. No OS
//                    notification; errors are usually only meaningful
//                    while the user is looking at the app.
//   NeedsUserInput — the app cannot proceed without the user, or the
//                    user is expected to accept/reject something. An
//                    in-app toast always fires. An OS-level
//                    notification fires *only when the application is
//                    not focused*. Persists until acknowledged.
//                    Appears in the top window under "Awaiting you".
//
// The focus gate exists because an OS-level notification on top of a
// visible app is noise. If the user is already looking at Lore, a toast
// is enough. If they are in another window, the OS notification is the
// only way to reach them.
//
// A notification may carry a target file path, a target card ID, and a
// target session name so that the top window and the OS notification
// click handler can route the user to the right place. Those fields are
// optional.
class NotificationService : public QObject {
  Q_OBJECT

public:
  enum class Severity {
    Info,
    Warning,
    Error,
    NeedsUserInput,
  };

  Q_ENUM(Severity)

  struct Notification {
    QString id;              // uuid, unique
    Severity severity = Severity::Info;
    QString title;
    QString body;
    QString targetFilePath;  // optional
    QString targetCardId;    // optional
    QString targetSessionName; // optional
    QDateTime createdAt;
    bool acknowledged = false;
  };

  static NotificationService &instance();

  // True when the application owns the focused top-level window. Used
  // by notify() to decide whether a NeedsUserInput notification also
  // fires an OS-level alert. Exposed as a free function so tests can
  // exercise the gate without touching the singleton.
  static bool applicationHasFocus();

  // Called once at app start with the ToastStack that will render
  // toasts. The service does not own the stack; it drives it. The
  // ToastStack must be a child of the top-level window so that toasts
  // are positioned correctly.
  void setToastHost(ToastStack *stack);

  // The primary entry point. Returns the id of the created
  // notification so that callers can later acknowledge it.
  QString notify(Severity severity, const QString &title,
                 const QString &body, const QString &targetFilePath = QString(),
                 const QString &targetCardId = QString(),
                 const QString &targetSessionName = QString());

  // Convenience wrappers. Use these in preference to notify() where the
  // severity is obvious.
  QString info(const QString &title, const QString &body,
               const QString &targetFilePath = QString());
  QString warning(const QString &title, const QString &body,
                  const QString &targetFilePath = QString());
  QString error(const QString &title, const QString &body,
                const QString &targetFilePath = QString());

  // A NeedsUserInput notification: the user must do something. Fires an
  // OS-level alert only when the application does not have focus.
  QString needsUserInput(const QString &title, const QString &body,
                         const QString &targetFilePath = QString(),
                         const QString &targetCardId = QString(),
                         const QString &targetSessionName = QString());

  // Acknowledge a NeedsUserInput notification. Removes it from
  // "Awaiting you" and stops the OS notification from re-firing.
  void acknowledge(const QString &id);

  // Acknowledge every NeedsUserInput notification whose target is the
  // given file. Used when a file's blocking condition resolves itself,
  // e.g. a scoped edit completes.
  void acknowledgeForFile(const QString &absolutePath);

  // Recent notifications, newest first. Includes every severity except
  // Info (Info expires and is not retained).
  QVector<Notification> recent() const;

  // NeedsUserInput notifications that have not been acknowledged. These
  // are what the top window shows under "Awaiting you."
  QVector<Notification> pending() const;

  // Settings-backed enable flags. Defaults are on. Changing one takes
  // effect on the next notification of that severity.
  bool toastsEnabled() const;
  void setToastsEnabled(bool enabled);

  bool osNotificationsEnabled() const;
  void setOsNotificationsEnabled(bool enabled);

  bool soundEnabled() const;
  void setSoundEnabled(bool enabled);

  int infoLifetimeMs() const;
  void setInfoLifetimeMs(int ms);

  int warningLifetimeMs() const;
  void setWarningLifetimeMs(int ms);

signals:
  // Emitted whenever the notification set changes: a new notification
  // arrives, one is acknowledged, or the settings change. The top
  // window listens to this and rebuilds its view.
  void changed();

  // Emitted when a notification is created. Used by anything that wants
  // to log or react. Distinct from changed() because changed() also
  // fires on acknowledgment.
  void notified(const NotificationService::Notification &notification);

private slots:
  void onToastDismissed(const QString &id);

private:
  explicit NotificationService(QObject *parent = nullptr);
  ~NotificationService() override;

  Q_DISABLE_COPY(NotificationService)

  void loadSettings();
  void saveSettings();

  void dispatchToast(const Notification &notification);
  void dispatchOsNotification(const Notification &notification);

  QString severityName(Severity severity) const;

  ToastStack *m_toastHost = nullptr;

  QVector<Notification> m_recent;
  QVector<Notification> m_pending;

  bool m_toastsEnabled = true;
  bool m_osNotificationsEnabled = true;
  bool m_soundEnabled = false;

  int m_infoLifetimeMs = 5000;
  int m_warningLifetimeMs = 12000;

  static constexpr int kMaxRecent = 200;
};