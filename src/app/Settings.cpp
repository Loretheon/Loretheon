#include "../../include/app/Settings.h"

#include <QDir>
#include <QSettings>
#include <QStandardPaths>

namespace {
constexpr auto OverseerToolCallDepthKey = "overseer/toolCallDepthLimit";
constexpr int DefaultToolCallDepth = 16;
} // namespace

QString Settings::getRootDirectory() {
  QSettings settings;
  const QString defaultRoot =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
      "/notes";
  QDir().mkpath(defaultRoot);
  return settings.value("root", defaultRoot).toString();
}

void Settings::setRootDirectory(const QString &newRoot) {
  QSettings settings;
  settings.setValue("root", newRoot);
}

int Settings::getOverseerToolCallDepthLimit() {
  QSettings settings;
  const int value = settings.value(OverseerToolCallDepthKey,
                                    DefaultToolCallDepth)
                        .toInt();

  return qBound(1, value, 64);
}

void Settings::setOverseerToolCallDepthLimit(int limit) {
  QSettings settings;
  settings.setValue(OverseerToolCallDepthKey, qBound(1, limit, 64));
}