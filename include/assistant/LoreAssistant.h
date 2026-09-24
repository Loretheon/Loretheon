#pragma once

#include "AssistantToolRegistry.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QUuid>

namespace assistant {
class AssistantToolRegistry;
}
class AssistantActivity;
class AssistantMemory;
class AssistantProfile;
class AssistantToolRegistry;
class AvatarWidget;
class DocumentArea;
class DocumentManager;
class InferenceService;
class MemoryIndex;
class SearchService;
class SpeechAnimator;

// The conductor. Owns the assistant's long-lived state, wires it to
// the rest of the application, and runs one LLM turn per user message.
//
// One turn: build a messages array from the profile and the user
// message, send it with the tool schemas, stream deltas back to the
// caller, execute any tool calls in a single round, and finish. The
// reply is spoken once at the end of the turn if the assistant's own
// settings allow it.
//
// No conversation history, no memory gate, no activity-driven turns.
// Those are later steps.
class LoreAssistant : public QObject {
  Q_OBJECT

public:
  struct Config {
    InferenceService *inference = nullptr;
    AvatarWidget *avatar = nullptr;
    DocumentManager *documents = nullptr;
    DocumentArea *documentArea = nullptr;
    SearchService *search = nullptr;

    QString root;
  };

  explicit LoreAssistant(const Config &config,
                         QObject *parent = nullptr);
  ~LoreAssistant() override;

  bool start();
  void stop();

  void say(const QString &text);

  void handleUserMessage(const QString &text);

  bool isTurnActive() const { return m_turnActive; }

  QString lastReply() const { return m_lastReply; }

  AssistantProfile *profile() const { return m_profile; }
  AssistantMemory *memory() const { return m_memory; }
  AssistantActivity *activity() const { return m_activity; }
  SpeechAnimator *animator() const { return m_animator; }
  assistant::AssistantToolRegistry *tools() const { return m_tools; }

signals:
  void assistantSaid(const QString &text);

  // Streaming reply text. Emitted many times per turn.
  void assistantChunk(const QString &text);

  // Status lines: tool activity, errors, refusals. Shown in the
  // transcript as italic lines and, for failures, as a notification.
  void assistantStatus(const QString &text);

  // A tool call began or ended. Emitted in addition to
  // assistantStatus so that a view can render a distinct indicator
  // without parsing the status text.
  void toolStarted(const QString &name);
  void toolFinished(const QString &name, bool ok, const QString &summary);

  // The turn is over.
  void assistantTurnFinished();

private slots:
  void onLlmDelta(const QUuid &token, const QString &text);
  void onLlmFinished(const QUuid &token);
  void onLlmToolCalls(const QUuid &token, const QJsonArray &toolCalls);
  void onLlmError(const QUuid &token, const QString &error);

private:
  QJsonArray buildMessages(const QString &userText) const;

  assistant::AssistantToolContext buildToolContext() const;

  void runToolRound(const QJsonArray &toolCalls,
                    const QJsonArray &priorMessages);

  void finishTurn();

  Config m_config;

  AssistantProfile *m_profile = nullptr;
  AssistantMemory *m_memory = nullptr;
  MemoryIndex *m_memoryIndex = nullptr;
  AssistantActivity *m_activity = nullptr;
  assistant::AssistantToolRegistry *m_tools = nullptr;
  SpeechAnimator *m_animator = nullptr;

  bool m_started = false;

  bool m_turnActive = false;
  QUuid m_turnToken;
  QJsonArray m_turnMessages;
  int m_toolRoundsRemaining = 1;

  QString m_turnReplyBuffer;
  QString m_lastReply;
};