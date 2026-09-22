#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>

class DocumentManager;
class TextDocument;
class TextEdit;

// A snapshot of everything the assistant can see about the current
// moment. Assembled fresh for every turn. Not stored.
//
// The context carries: which file is open, which scope the cursor is
// in, the last few user actions from the activity stream, the last
// assistant reply, and the last user message. Everything else the
// assistant needs comes from the profile or from a memory recall.
struct AssistantContext {
  // The file currently open in the editor, or empty.
  QString openFilePath;

  // The scope id the cursor is in, if any.
  QString cursorScopeId;

  // The last few activity events as short human-readable strings.
  QStringList recentActivity;

  // The last user message and the last assistant reply, if any.
  QString lastUserMessage;
  QString lastAssistantReply;

  // The wall clock, UTC.
  QDateTime now;

  // Build from the live application. Any of the pointers may be null.
  static AssistantContext snapshot(DocumentManager *documents,
                                   TextEdit *editor,
                                   const QStringList &activity,
                                   const QString &lastUser,
                                   const QString &lastReply);

  // Render as a compact prompt section. Never longer than a few
  // hundred characters, because this goes into every prompt.
  QString toPromptSection() const;
};