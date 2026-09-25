#pragma once

#include "AssistantToolRegistry.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QQueue>
#include <QString>
#include <QStringList>
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
class NotePromoter;
class OverseerSessionManager;
class RetrievalLoop;
class ScopeIndex;
class SearchService;
class SpeechAnimator;

// The conductor. Owns the assistant's long-lived state and runs one
// LLM turn per user message.
//
// The assistant does not do work. She submits jobs and speaks. Search
// runs through her own RetrievalLoop. Anything that produces or
// modifies notes goes to OverseerSessionManager, which runs it in a
// session and reports back through requestFinished. The conversation
// stays free while the work happens.
//
// When a job completes, the completion policy chosen by the user
// decides what happens to the result: paste it in the chat, feed it
// to the assistant's next prompt, append it to the user's next
// message, or let the assistant decide based on whether the user is
// mid-turn.
class LoreAssistant : public QObject {
  Q_OBJECT

public:
  struct Config {
    InferenceService *inference = nullptr;
    AvatarWidget *avatar = nullptr;
    DocumentManager *documents = nullptr;
    DocumentArea *documentArea = nullptr;
    SearchService *search = nullptr;
    OverseerSessionManager *overseerManager = nullptr;
    NotePromoter *promoter = nullptr;
    ScopeIndex *scopeIndex = nullptr;

    QString root;
  };

  enum class CompletionPolicy {
    PasteInChat,
    FeedToQueue,
    AppendToNextUserMessage,
    Automatic,
  };
  Q_ENUM(CompletionPolicy)

  explicit LoreAssistant(const Config &config,
                         QObject *parent = nullptr);
  ~LoreAssistant() override;

  bool start();
  void stop();

  void say(const QString &text);

  void handleUserMessage(const QString &text);

  bool isTurnActive() const { return m_turnActive; }

  QString lastReply() const { return m_lastReply; }

  CompletionPolicy completionPolicy() const { return m_completionPolicy; }
  void setCompletionPolicy(CompletionPolicy policy);

  AssistantProfile *profile() const { return m_profile; }
  AssistantMemory *memory() const { return m_memory; }
  AssistantActivity *activity() const { return m_activity; }
  SpeechAnimator *animator() const { return m_animator; }
  assistant::AssistantToolRegistry *tools() const { return m_tools; }

signals:
  void assistantSaid(const QString &text);

  void assistantChunk(const QString &text);

  void assistantStatus(const QString &text);

  // Emitted when a background job's result is pasted into the chat,
  // and when a delegated job completes regardless of policy, so the
  // panel can show a status line.
  void jobCompleted(const QString &summary);

  void assistantTurnFinished();

private slots:
  void onLlmDelta(const QUuid &token, const QString &text);
  void onLlmFinished(const QUuid &token);
  void onLlmToolCalls(const QUuid &token, const QJsonArray &toolCalls);
  void onLlmError(const QUuid &token, const QString &error);

  void onOverseerRequestFinished(const QString &sessionName,
                                 const QString &requestId, bool ok,
                                 const QString &summary,
                                 const QString &filePath);

private:
  QString runSearchForTool(const QString &query);

  QJsonArray buildMessages(const QString &userText);

  assistant::AssistantToolContext buildToolContext();

  void runToolRound(const QJsonArray &toolCalls,
                    const QJsonArray &priorMessages);

  void finishTurn();

  void applyCompletionPolicy(const QString &summary);

  QString systemPrompt() const;

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
  int m_toolRoundsRemaining = 5;

  QString m_turnReplyBuffer;
  QString m_lastReply;

  CompletionPolicy m_completionPolicy = CompletionPolicy::Automatic;

  // Job bookkeeping. Keyed by the id returned from submit.
  QHash<QString, CompletionPolicy> m_pendingJobs;

  // The session each pending job belongs to, for provenance when the
  // result comes back.
  QHash<QString, QString> m_jobSessions;

  // Results waiting to be fed to the next prompt or appended to the
  // next user message.
  QStringList m_pendingForPrompt;
  QStringList m_pendingForUserMessage;

  // The assistant's private search pipeline. One run at a time; a new
  // query cancels the previous one.
  RetrievalLoop *m_searchLoop = nullptr;
  QString m_searchAnswerBuffer;
  bool m_searchInFlight = false;
};