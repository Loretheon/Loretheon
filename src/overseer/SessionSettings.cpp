#include "../../include/overseer/SessionSettings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>

void SessionSettings::normalize() {
  if (automatic) {
    autoMemory = true;
    autoEdits = true;
  }
}

SessionSettings SessionSettings::load(const QString &path) {
  SessionSettings settings;

  if (path.isEmpty()) {
    settings.normalize();
    return settings;
  }

  QFile file(path);

  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    settings.normalize();
    return settings;
  }

  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());

  if (!doc.isObject()) {
    settings.normalize();
    return settings;
  }

  const QJsonObject obj = doc.object();

  settings.automatic = obj.value(QStringLiteral("automatic")).toBool(false);
  settings.autoMemory = obj.value(QStringLiteral("autoMemory")).toBool(false);
  settings.autoEdits = obj.value(QStringLiteral("autoEdits")).toBool(false);

  settings.normalize();
  return settings;
}

bool SessionSettings::save(const QString &path) const {
  if (path.isEmpty())
    return false;

  SessionSettings copy = *this;
  copy.normalize();

  const QFileInfo info(path);
  const QString dirPath = info.absolutePath();

  if (!dirPath.isEmpty()) {
    QDir dir;

    if (!dir.exists(dirPath) && !dir.mkpath(dirPath))
      return false;
  }

  QJsonObject obj;
  obj.insert(QStringLiteral("automatic"), copy.automatic);
  obj.insert(QStringLiteral("autoMemory"), copy.autoMemory);
  obj.insert(QStringLiteral("autoEdits"), copy.autoEdits);

  QFile file(path);

  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate |
                 QIODevice::Text))
    return false;

  const QByteArray payload =
      QJsonDocument(obj).toJson(QJsonDocument::Indented);

  if (file.write(payload) != payload.size())
    return false;

  file.flush();
  file.close();

  return true;
}

bool SessionSettings::ensureFile(const QString &path) {
  if (path.isEmpty())
    return false;

  if (QFileInfo::exists(path))
    return true;

  SessionSettings defaults;
  return defaults.save(path);
}