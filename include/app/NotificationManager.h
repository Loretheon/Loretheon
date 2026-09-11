#pragma once

#include <QObject>
#include <QString>

class QSystemTrayIcon;

class NotificationManager : public QObject {
  Q_OBJECT

public:
  explicit NotificationManager(QObject *parent = nullptr);

  void notify(const QString &title, const QString &message,
              int timeoutMs = 5000);

private:
  QSystemTrayIcon *m_trayIcon = nullptr;
};