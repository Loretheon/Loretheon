#include <QApplication>
#include <iostream>
#include <qsettings.h>

#include "../include/app/MainWindow.h"
#include "Settings.h"

#ifdef LORE_WITH_AVATAR
#include "AvatarSurface.h"
#include <qqml.h>
#endif

int main(int argc, char **argv) {
  QApplication app(argc, argv);

#ifdef LORE_WITH_AVATAR
  qmlRegisterType<AvatarSurface>("Lore.Avatar", 1, 0, "AvatarSurface");
#endif

  QCoreApplication::setOrganizationName("Questfarer");
  QCoreApplication::setApplicationName("Lore");
  QCoreApplication::setApplicationVersion("0.0");
  const QString root = Settings::getRootDirectory();
  MainWindow window;
  window.show();
  return app.exec();
}