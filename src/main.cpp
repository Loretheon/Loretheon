#include <iostream>
#include <QApplication>

#include "../include/app/MainWindow.h"

int main(int argc, char ** argv) {
    QApplication app (argc, argv);
    QCoreApplication::setOrganizationName("QuestFarer");
    QCoreApplication::setApplicationName("Episteme");
    QCoreApplication::setApplicationVersion("0.0");

    MainWindow window;
    window.show();
    return app.exec();
}
