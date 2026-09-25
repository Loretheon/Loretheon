#include "../../include/overseer/SessionSettings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

SessionSettings SessionSettings::load(const QString &path) {
  SessionSettings settings;

  QFile file(path);

  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return settings;
  }

  const QByteArray data = file.readAll();
  file.close();

  const QJsonDocument doc = QJsonDocument::fromJson(data);

  if (!doc.isObject()) {
    return settings;
  }

  const QJsonObject obj = doc.object();

  settings.automatic =
      obj.value(QStringLiteral("automatic")).toBool(false);
  settings.autoMemory =
      obj.value(QStringLiteral("autoMemory")).toBool(false);
  settings.autoEdits =
      obj.value(QStringLiteral("autoEdits")).toBool(false);
  settings.description =
      obj.value(QStringLiteral("description")).toString();

  settings.normalize();

  return settings;
}

bool SessionSettings::save(const QString &path) const {
  SessionSettings copy = *this;
  copy.normalize();

  const QFileInfo info(path);
  const QDir parent = info.absoluteDir();

  if (!parent.exists() && !QDir().mkpath(parent.absolutePath())) {
    return false;
  }

  QJsonObject obj;
  obj.insert(QStringLiteral("automatic"), copy.automatic);
  obj.insert(QStringLiteral("autoMemory"), copy.autoMemory);
  obj.insert(QStringLiteral("autoEdits"), copy.autoEdits);
  obj.insert(QStringLiteral("description"), copy.description);

  QFile file(path);

  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate |
                 QIODevice::Text)) {
    return false;
  }

  file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
  file.close();

  return true;
}

bool SessionSettings::ensureFile(const QString &path) {
  if (QFileInfo::exists(path)) {
    return true;
  }

  SessionSettings defaults;
  return defaults.save(path);
}

void SessionSettings::normalize() {
  if (automatic) {
    autoMemory = true;
    autoEdits = true;
  }
}