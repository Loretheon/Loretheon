#include <QApplication>
#include <iostream>
#include <qsettings.h>

#include "../include/app/MainWindow.h"
#include "Settings.h"

int main(int argc, char **argv) {
  QApplication app(argc, argv);
  QCoreApplication::setOrganizationName("QuestFarer");
  QCoreApplication::setApplicationName("Episteme");
  QCoreApplication::setApplicationVersion("0.0");
  const QString root = Settings::getRootDirectory();
  MainWindow window;
  window.show();
  return app.exec();
}
