#ifndef EPISTEME_TEXTWIDGET_H
#define EPISTEME_TEXTWIDGET_H

#include <QTabWidget>
#include <QTextDocument>

#include "TextBrowser.h"
#include "TextEdit.h"
#include "TextDocument.h"

class TextWidget : public QTabWidget
{
    Q_OBJECT

public:
    explicit TextWidget(QWidget *parent = nullptr);

public slots:
    void setActiveDocument(TextDocument *newDocument);

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

private slots:
    void syncPreview();

private:
    TextBrowser *textBrowser;
    TextEdit *textEdit;

    QTextDocument *previewDocument = nullptr;
    TextDocument *activeDocument = nullptr;
};

#endif //EPISTEME_TEXTWIDGET_H