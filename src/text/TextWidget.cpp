#include "TextWidget.h"

#include <QTextCursor>
#include <QTextCharFormat>


TextWidget::TextWidget(QWidget* parent)
    : QTabWidget(parent)
{
    textBrowser = new TextBrowser(this);
    textEdit = new TextEdit(this);

    addTab(textEdit, "edit");
    addTab(textBrowser, "view");

    previewDocument = new QTextDocument(this);
    textBrowser->setDocument(previewDocument);
}


void TextWidget::setActiveDocument(TextDocument* newDocument)
{
    if (activeDocument)
        disconnect(activeDocument, &QTextDocument::contentsChanged, this, &TextWidget::syncPreview);

    activeDocument = newDocument;

    textEdit->setDocument(newDocument);

    if (!newDocument)
        return;

    if (newDocument->type() == TextDocument::Type::Markdown)
    {
        connect(newDocument, &QTextDocument::contentsChanged, this, &TextWidget::syncPreview);
        syncPreview();
    }
    else
    {
        textBrowser->setDocument(newDocument);
    }
}

void TextWidget::syncPreview()
{
    if (!activeDocument || activeDocument->type() != TextDocument::Type::Markdown)
        return;

    if (textBrowser->document() != previewDocument)
        textBrowser->setDocument(previewDocument);

    previewDocument->setMarkdown(activeDocument->toPlainText());
}


void TextWidget::undo()
{
    textEdit->undo();
}


void TextWidget::redo()
{
    textEdit->redo();
}


void TextWidget::cut()
{
    textEdit->cut();
}


void TextWidget::copy()
{
    textEdit->copy();
}


void TextWidget::paste()
{
    textEdit->paste();
}


void TextWidget::bold()
{
    textEdit->bold();
}


void TextWidget::italic()
{
    textEdit->italic();
}


void TextWidget::leftAlign()
{
    textEdit->leftAlign();
}


void TextWidget::rightAlign()
{
    textEdit->rightAlign();
}


void TextWidget::justify()
{
    textEdit->justify();
}


void TextWidget::center()
{
    textEdit->center();
}


void TextWidget::setLineSpacing()
{
    textEdit->setLineSpacing();
}


void TextWidget::setParagraphSpacing()
{
    textEdit->setParagraphSpacing();
}