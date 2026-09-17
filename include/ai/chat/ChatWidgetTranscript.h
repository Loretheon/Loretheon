#pragma once

#include <QString>
#include <QTextCursor>

class QTextEdit;

class ChatWidgetTranscript {
public:
  static void appendUserMessage(QTextEdit *transcript, const QString &text);

  static void appendAssistantChunk(QTextEdit *transcript, const QString &text,
                                   bool &assistantMessageOpen);

  static void appendStatusMessage(QTextEdit *transcript, const QString &text);

  static void renderLastAssistantMessage(QTextEdit *transcript);

private:
  static void appendMessageSeparator(QTextCursor &cursor);
};