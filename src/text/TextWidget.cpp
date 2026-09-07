#include "TextWidget.h"

#include <QTextCursor>
#include <QTextCharFormat>


TextWidget::TextWidget(QWidget* parent)
    : QTabWidget(parent)
{
    textBrowser = new TextBrowser;
    textEdit = new TextEdit;

    addTab(textBrowser, "view");
    addTab(textEdit, "edit");
}


void TextWidget::setActiveDocument(TextDocument* newDocument)
{
    textBrowser->setDocument(newDocument);
    textEdit->setDocument(newDocument);
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
    QTextCharFormat format;
    format.setFontWeight(QFont::Bold);

    textEdit->textCursor().mergeCharFormat(format);
}


void TextWidget::italic()
{
    QTextCharFormat format;
    format.setFontItalic(true);

    textEdit->textCursor().mergeCharFormat(format);
}


void TextWidget::leftAlign()
{
    textEdit->setAlignment(Qt::AlignLeft);
}


void TextWidget::rightAlign()
{
    textEdit->setAlignment(Qt::AlignRight);
}


void TextWidget::justify()
{
    textEdit->setAlignment(Qt::AlignJustify);
}


void TextWidget::center()
{
    textEdit->setAlignment(Qt::AlignCenter);
}


void TextWidget::setLineSpacing()
{
    // Placeholder:
    // requires QTextBlockFormat and QTextCursor
}


void TextWidget::setParagraphSpacing()
{
    // Placeholder:
    // requires QTextBlockFormat and QTextCursor
}