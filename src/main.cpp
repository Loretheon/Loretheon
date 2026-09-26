#include <QApplication>
#include <iostream>
#include <qsettings.h>

#include "../include/app/MainWindow.h"
#include "AvatarSurface.h"
#include "Settings.h"

#include <QDirIterator>
#include <qqml.h>

int main(int argc, char **argv) {

  // QDirIterator it(":", QDirIterator::Subdirectories);
  // while (it.hasNext()) {
  //   qDebug() << it.next();
  // }
  qmlRegisterType<AvatarSurface>("Lore.Avatar", 1, 0, "AvatarSurface");
  QApplication app(argc, argv);
  QCoreApplication::setOrganizationName("Questfarer");
  QCoreApplication::setApplicationName("Lore");
  QCoreApplication::setApplicationVersion("0.0");
  const QString root = Settings::getRootDirectory();
  MainWindow window;
  window.show();
  return app.exec();
}
