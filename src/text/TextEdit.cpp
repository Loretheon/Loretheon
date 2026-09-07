#include "TextEdit.h"

#include <QTextCursor>
#include <QTextCharFormat>
#include <QTextBlockFormat>


TextEdit::TextEdit(QWidget *parent)
    : QTextEdit(parent)
{
}


void TextEdit::bold()
{
    QTextCharFormat format;
    format.setFontWeight(QFont::Bold);

    textCursor().mergeCharFormat(format);
}


void TextEdit::italic()
{
    QTextCharFormat format;
    format.setFontItalic(true);

    textCursor().mergeCharFormat(format);
}


void TextEdit::leftAlign()
{
    setAlignment(Qt::AlignLeft);
}


void TextEdit::rightAlign()
{
    setAlignment(Qt::AlignRight);
}


void TextEdit::justify()
{
    setAlignment(Qt::AlignJustify);
}


void TextEdit::center()
{
    setAlignment(Qt::AlignCenter);
}


void TextEdit::setLineSpacing()
{
    QTextCursor cursor = textCursor();

    QTextBlockFormat format;
    format.setLineHeight(
        120,
        QTextBlockFormat::ProportionalHeight
    );

    cursor.mergeBlockFormat(format);
    setTextCursor(cursor);
}


void TextEdit::setParagraphSpacing()
{
    QTextCursor cursor = textCursor();

    QTextBlockFormat format;
    format.setBottomMargin(10);

    cursor.mergeBlockFormat(format);
    setTextCursor(cursor);
}