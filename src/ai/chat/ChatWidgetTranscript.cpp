#include "../../../include/ai/chat/ChatWidgetTranscript.h"

#include <QFont>
#include <QTextBlockFormat>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>

namespace {

constexpr int MessageSpacing = 4;
constexpr int StatusSpacing = 2;

QTextBlockFormat messageBlockFormat(int topMargin = 0,
                                     int bottomMargin = MessageSpacing) {
  QTextBlockFormat format;
  format.setTopMargin(topMargin);
  format.setBottomMargin(bottomMargin);
  format.setLineHeight(110, QTextBlockFormat::ProportionalHeight);
  return format;
}

QTextBlockFormat statusBlockFormat() {
  QTextBlockFormat format =
      messageBlockFormat(StatusSpacing, StatusSpacing);
  format.setLeftMargin(0);
  format.setRightMargin(0);
  return format;
}

QTextCharFormat senderFormat() {
  QTextCharFormat format;
  format.setFontWeight(QFont::Normal);
  format.setFontPointSize(9);
  return format;
}

QTextCharFormat statusFormat() {
  QTextCharFormat format;
  format.setFontItalic(true);
  format.setFontPointSize(8);
  return format;
}

} // namespace

void ChatWidgetTranscript::appendMessageSeparator(QTextCursor &cursor) {
  QTextBlockFormat format;
  format.setTopMargin(0);
  format.setBottomMargin(0);
  format.setLeftMargin(0);
  format.setRightMargin(0);
  cursor.insertBlock(format);
}

void ChatWidgetTranscript::appendUserMessage(QTextEdit *transcript,
                                             const QString &text) {
  if (!transcript) {
    return;
  }

  QTextCursor cursor = transcript->textCursor();
  cursor.movePosition(QTextCursor::End);

  if (!cursor.atBlockStart() && !transcript->document()->isEmpty()) {
    appendMessageSeparator(cursor);
  }

  cursor.insertBlock(messageBlockFormat());
  cursor.insertText(text);

  transcript->setTextCursor(cursor);
  transcript->ensureCursorVisible();
}

void ChatWidgetTranscript::appendAssistantChunk(QTextEdit *transcript,
                                                const QString &text,
                                                bool &assistantMessageOpen) {
  if (!transcript) {
    return;
  }

  QTextCursor cursor = transcript->textCursor();
  cursor.movePosition(QTextCursor::End);

  if (!assistantMessageOpen) {
    if (!cursor.atBlockStart() && !transcript->document()->isEmpty()) {
      appendMessageSeparator(cursor);
    }

    cursor.insertBlock(messageBlockFormat());
    assistantMessageOpen = true;
  }

  cursor.movePosition(QTextCursor::End);
  cursor.insertText(text);

  transcript->setTextCursor(cursor);
  transcript->ensureCursorVisible();
}

void ChatWidgetTranscript::appendStatusMessage(QTextEdit *transcript,
                                               const QString &text) {
  if (!transcript) {
    return;
  }

  QTextCursor cursor = transcript->textCursor();
  cursor.movePosition(QTextCursor::End);

  if (!cursor.atBlockStart() && !transcript->document()->isEmpty()) {
    appendMessageSeparator(cursor);
  }

  cursor.insertBlock(statusBlockFormat());
  cursor.insertText(text, statusFormat());

  transcript->setTextCursor(cursor);
  transcript->ensureCursorVisible();
}

void ChatWidgetTranscript::renderLastAssistantMessage(QTextEdit *transcript) {
  if (!transcript) {
    return;
  }

  transcript->ensureCursorVisible();
}