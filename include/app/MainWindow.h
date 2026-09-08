#ifndef EPISTEME_MAINWINDOW_H
#define EPISTEME_MAINWINDOW_H

#include <QMainWindow>

#include "DocumentManager.h"
#include "TextWidget.h"
#include "FileWidget.h"

class QMenu;
class QAction;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow();

private:
    void createActions();
    void createMenus();

    QMenu *fileMenu = nullptr;
    QMenu *newMenu = nullptr;
    QMenu *helpMenu = nullptr;

    QAction *newTextAct = nullptr;
    QAction *newMarkdownAct = nullptr;
    QAction *openAct = nullptr;
    QAction *saveAct = nullptr;
    QAction *exitAct = nullptr;
    QAction *aboutAct = nullptr;
    QAction *aboutQtAct = nullptr;

    DocumentManager *documentManager = nullptr;
    TextWidget *textWidget = nullptr;
    FileWidget *fileWidget = nullptr;

private slots:
    void about();
    void aboutQt();
};

#endif // EPISTEME_MAINWINDOW_H