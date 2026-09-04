#ifndef EPISTEME_MAINWINDOW_H
#define EPISTEME_MAINWINDOW_H
#include <QMainWindow>
#include <QLabel>

#include "TextBrowser.h"
#include "TextDocument.h"
#include "TextEdit.h"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow();

protected:
#ifndef QT_NO_CONTEXTMENU
    void contextMenuEvent(QContextMenuEvent *event) override;
#endif // QT_NO_CONTEXTMENU

private slots:
    void newFile();

    void open();

    void save();

    void print();

    void undo();

    void redo();

    void cut();

    void copy();

    void paste();

    void bold();

    void italic();

    void leftAlign();

    void rightAlign();

    void justify();

    void center();

    void setLineSpacing();

    void setParagraphSpacing();

    void about();

    void aboutQt();

private:
    void createActions();

    void createMenus();

    QMenu *fileMenu;
    QMenu *editMenu;
    QMenu *formatMenu;
    QMenu *helpMenu;
    QActionGroup *alignmentGroup;
    QAction *newAct;
    QAction *openAct;
    QAction *saveAct;
    QAction *printAct;
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
    TextDocument *textDocument;
    TextBrowser *textBrowser;
    TextEdit *textEdit;
};


#endif //EPISTEME_MAINWINDOW_H
