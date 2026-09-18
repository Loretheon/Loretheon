#include "../../include/app/NotificationService.h"

#include "ToastStack.h"

#include <QApplication>
#include <QDateTime>
#include <QDebug>
#include <QSettings>
#include <QSystemTrayIcon>
#include <QUuid>

namespace {

constexpr auto SettingsToasts = "notifications/toastsEnabled";
constexpr auto SettingsOs = "notifications/osEnabled";
constexpr auto SettingsSound = "notifications/soundEnabled";
constexpr auto SettingsInfoLifetime = "notifications/infoLifetimeMs";
constexpr auto SettingsWarningLifetime = "notifications/warningLifetimeMs";

QSystemTrayIcon *ensureTray() {
  static QSystemTrayIcon *tray = nullptr;

  if (tray)
    return tray;

  if (!QSystemTrayIcon::isSystemTrayAvailable())
    return nullptr;

  tray = new QSystemTrayIcon(QApplication::windowIcon(),
                             qApp);
  tray->setToolTip(QStringLiteral("Lore"));
  tray->show();

  return tray;
}

} // namespace

NotificationService &NotificationService::instance() {
  static NotificationService service;
  return service;
}

NotificationService::NotificationService(QObject *parent) : QObject(parent) {
  loadSettings();
}

NotificationService::~NotificationService() = default;

void NotificationService::setToastHost(ToastStack *stack) {
  m_toastHost = stack;
}

QString NotificationService::notify(Severity severity, const QString &title,
                                    const QString &body,
                                    const QString &targetFilePath,
                                    const QString &targetCardId) {
  Notification notification;
  notification.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  notification.severity = severity;
  notification.title = title;
  notification.body = body;
  notification.targetFilePath = targetFilePath;
  notification.targetCardId = targetCardId;
  notification.createdAt = QDateTime::currentDateTime();

  if (severity != Severity::Info) {
    m_recent.prepend(notification);

    while (m_recent.size() > kMaxRecent)
      m_recent.removeLast();
  }

  if (severity == Severity::Critical)
    m_pending.prepend(notification);

  if (m_toastsEnabled)
    dispatchToast(notification);

  if (severity == Severity::Critical && m_osNotificationsEnabled)
    dispatchOsNotification(notification);

  emit notified(notification);
  emit changed();

  return notification.id;
}

QString NotificationService::info(const QString &title, const QString &body,
                                  const QString &targetFilePath) {
  return notify(Severity::Info, title, body, targetFilePath);
}

QString NotificationService::warning(const QString &title, const QString &body,
                                     const QString &targetFilePath) {
  return notify(Severity::Warning, title, body, targetFilePath);
}

QString NotificationService::error(const QString &title, const QString &body,
                                   const QString &targetFilePath) {
  return notify(Severity::Error, title, body, targetFilePath);
}

QString NotificationService::critical(const QString &title, const QString &body,
                                      const QString &targetFilePath,
                                      const QString &targetCardId) {
  return notify(Severity::Critical, title, body, targetFilePath,
                targetCardId);
}

void NotificationService::acknowledge(const QString &id) {
  if (id.isEmpty())
    return;

  bool changedAny = false;

  for (int i = m_pending.size() - 1; i >= 0; --i) {
    if (m_pending.at(i).id != id)
      continue;

    m_pending.removeAt(i);
    changedAny = true;
  }

  for (Notification &notification : m_recent) {
    if (notification.id != id)
      continue;

    notification.acknowledged = true;
    changedAny = true;
  }

  if (changedAny)
    emit changed();
}

void NotificationService::acknowledgeForFile(const QString &absolutePath) {
  if (absolutePath.isEmpty())
    return;

  QVector<QString> toAcknowledge;

  for (const Notification &notification : std::as_const(m_pending)) {
    if (notification.targetFilePath == absolutePath)
      toAcknowledge.append(notification.id);
  }

  if (toAcknowledge.isEmpty())
    return;

  for (const QString &id : std::as_const(toAcknowledge))
    acknowledge(id);
}

QVector<NotificationService::Notification>
NotificationService::recent() const {
  return m_recent;
}

QVector<NotificationService::Notification>
NotificationService::pending() const {
  return m_pending;
}

bool NotificationService::toastsEnabled() const { return m_toastsEnabled; }

void NotificationService::setToastsEnabled(bool enabled) {
  if (m_toastsEnabled == enabled)
    return;

  m_toastsEnabled = enabled;
  saveSettings();
  emit changed();
}

bool NotificationService::osNotificationsEnabled() const {
  return m_osNotificationsEnabled;
}

void NotificationService::setOsNotificationsEnabled(bool enabled) {
  if (m_osNotificationsEnabled == enabled)
    return;

  m_osNotificationsEnabled = enabled;
  saveSettings();
  emit changed();
}

bool NotificationService::soundEnabled() const { return m_soundEnabled; }

void NotificationService::setSoundEnabled(bool enabled) {
  if (m_soundEnabled == enabled)
    return;

  m_soundEnabled = enabled;
  saveSettings();
  emit changed();
}

int NotificationService::infoLifetimeMs() const { return m_infoLifetimeMs; }

void NotificationService::setInfoLifetimeMs(int ms) {
  if (ms <= 0 || m_infoLifetimeMs == ms)
    return;

  m_infoLifetimeMs = ms;
  saveSettings();
  emit changed();
}

int NotificationService::warningLifetimeMs() const {
  return m_warningLifetimeMs;
}

void NotificationService::setWarningLifetimeMs(int ms) {
  if (ms <= 0 || m_warningLifetimeMs == ms)
    return;

  m_warningLifetimeMs = ms;
  saveSettings();
  emit changed();
}

void NotificationService::onToastDismissed(const QString &id) {
  // Dismissal of a toast does not acknowledge a critical notification.
  // Critical notifications are only acknowledged by explicit action.
  Q_UNUSED(id);
}

void NotificationService::loadSettings() {
  QSettings settings;

  m_toastsEnabled = settings.value(SettingsToasts, true).toBool();
  m_osNotificationsEnabled = settings.value(SettingsOs, true).toBool();
  m_soundEnabled = settings.value(SettingsSound, false).toBool();

  m_infoLifetimeMs =
      settings.value(SettingsInfoLifetime, m_infoLifetimeMs).toInt();
  m_warningLifetimeMs =
      settings.value(SettingsWarningLifetime, m_warningLifetimeMs).toInt();
}

void NotificationService::saveSettings() {
  QSettings settings;

  settings.setValue(SettingsToasts, m_toastsEnabled);
  settings.setValue(SettingsOs, m_osNotificationsEnabled);
  settings.setValue(SettingsSound, m_soundEnabled);
  settings.setValue(SettingsInfoLifetime, m_infoLifetimeMs);
  settings.setValue(SettingsWarningLifetime, m_warningLifetimeMs);
}

void NotificationService::dispatchToast(const Notification &notification) {
  if (!m_toastHost)
    return;

  int lifetimeMs = 0;

  switch (notification.severity) {
  case Severity::Info:
    lifetimeMs = m_infoLifetimeMs;
    break;
  case Severity::Warning:
  case Severity::Error:
  case Severity::Critical:
    lifetimeMs = m_warningLifetimeMs;
    break;
  }

  m_toastHost->showNotification(notification.id, notification.title,
                                notification.body, notification.severity,
                                lifetimeMs);
}

void NotificationService::dispatchOsNotification(
    const Notification &notification) {
  QSystemTrayIcon *tray = ensureTray();

  if (!tray)
    return;

  const QString text =
      notification.body.isEmpty() ? notification.title
                                  : notification.title + QStringLiteral("\n") +
                                        notification.body;

  tray->showMessage(QStringLiteral("Lore"), text,
                    QSystemTrayIcon::Critical, 10000);
}

QString NotificationService::severityName(Severity severity) const {
  switch (severity) {
  case Severity::Info:
    return QStringLiteral("info");
  case Severity::Warning:
    return QStringLiteral("warning");
  case Severity::Error:
    return QStringLiteral("error");
  case Severity::Critical:
    return QStringLiteral("critical");
  }

  return QStringLiteral("info");
}