#include "../../include/app/Settings.h"

#include <qdir.h>
#include <qsettings.h>
#include <qstandardpaths.h>

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
