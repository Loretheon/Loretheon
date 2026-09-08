#ifndef EPISTEME_MAINWINDOW_H
#define EPISTEME_MAINWINDOW_H
#include <QMainWindow>
#include <QLabel>

#include "DocumentManager.h"
#include "TextBrowser.h"
#include "TextDocument.h"
#include "TextEdit.h"
#include "TextWidget.h"
#include "FileWidget.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow();

protected:
#ifndef QT_NO_CONTEXTMENU
    void contextMenuEvent(QContextMenuEvent *event) override;
#endif // QT_NO_CONTEXTMENU


private:
    void createActions();

    void createMenus();

    QMenu *fileMenu;
    QMenu *newMenu;
    QMenu *editMenu;
    QMenu *formatMenu;
    QMenu *helpMenu;
    QActionGroup *alignmentGroup;
    QAction *newTextAct;
    QAction *newMarkdownAct;
    QAction *openAct;
    QAction *saveAct;
    QAction *exitAct;
    QAction *undoAct;
    QAction *redoAct;
    QAction *cutAct;
    QAction *copyAct;
    QAction *pasteAct;
    QAction *boldAct;
    QAction *italicAct;
    QAction *leftAlignAct;
    QAction *rightAlignAct;
    QAction *justifyAct;
    QAction *centerAct;
    QAction *setLineSpacingAct;
    QAction *setParagraphSpacingAct;
    QAction *aboutAct;
    QAction *aboutQtAct;

    DocumentManager* documentManager;

    TextWidget* textWidget;
    FileWidget* fileWidget;

private slots :

    void about();

    void aboutQt();

};


#endif //EPISTEME_MAINWINDOW_H