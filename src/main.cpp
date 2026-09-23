#include <QApplication>
#include <iostream>
#include <qsettings.h>

#include "../include/app/MainWindow.h"
#include "Settings.h"

#include <QDirIterator>

int main(int argc, char **argv) {

  // QDirIterator it(":", QDirIterator::Subdirectories);
  // while (it.hasNext()) {
  //   qDebug() << it.next();
  // }
  QApplication app(argc, argv);
  QCoreApplication::setOrganizationName("Questfarer");
  QCoreApplication::setApplicationName("Lore");
  QCoreApplication::setApplicationVersion("0.0");
  const QString root = Settings::getRootDirectory();
  MainWindow window;
  window.show();
  return app.exec();
}
