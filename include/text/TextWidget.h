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

    TextEdit* editor() const { return textEdit; }
    TextBrowser* browser() const { return textBrowser; }

public slots:
    void setActiveDocument(TextDocument *newDocument);

private slots:
    void syncPreview();

private:
    TextBrowser *textBrowser = nullptr;
    TextEdit *textEdit = nullptr;

    QTextDocument *previewDocument = nullptr;
    TextDocument *activeDocument = nullptr;
};

#endif // EPISTEME_TEXTWIDGET_H