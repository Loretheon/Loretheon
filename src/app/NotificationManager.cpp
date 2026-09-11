#include "NotificationManager.h"

#include <QApplication>
#include <QIcon>
#include <QSystemTrayIcon>

NotificationManager::NotificationManager(QObject *parent) : QObject(parent) {

  if (!QSystemTrayIcon::isSystemTrayAvailable()) {
    return;
  }

  m_trayIcon = new QSystemTrayIcon(this);

  QIcon icon = qApp->windowIcon();

  if (!icon.isNull()) {
    m_trayIcon->setIcon(icon);
  }
}

void NotificationManager::notify(const QString &title, const QString &message,
                                 int timeoutMs) {

  if (!m_trayIcon) {
    return;
  }

  if (!m_trayIcon->isVisible()) {
    m_trayIcon->show();
  }

  m_trayIcon->showMessage(title, message, QSystemTrayIcon::Information,
                          timeoutMs);
}