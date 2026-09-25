#pragma once

#include "AssistantToolRegistry.h"
#include "ChatNode.h"

#include <QHash>
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
class NoteEditJob;
class NotePromoter;
class OverseerSessionManager;
class RetrievalLoop;
class ScopeIndex;
class SearchService;
class SpeechAnimator;

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

    QString notesRoot;
    QString root;
  };

  enum class CompletionPolicy {
    PasteInChat,
    FeedToQueue,
    AppendToNextUserMessage,
    Automatic,
  };
  Q_ENUM(CompletionPolicy)

  struct Job {
    QString id;
    ChatNode::Kind kind = ChatNode::Kind::JobSearch;
    ChatNode::State state = ChatNode::State::Pending;
    QString nodeId;
    QString summary;
    QString result;
    QString error;
    QDateTime createdAt;
    QDateTime updatedAt;

    bool isTerminal() const {
      return state == ChatNode::State::Done ||
             state == ChatNode::State::Failed ||
             state == ChatNode::State::Cancelled;
    }
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
  QString rootPath() const { return m_config.root; }

  CompletionPolicy completionPolicy() const { return m_completionPolicy; }
  void setCompletionPolicy(CompletionPolicy policy);

  const Job *job(const QString &jobId) const;
  QStringList jobIds() const;

  void abortJob(const QString &jobId);
  void abortAll();

  bool waitForJob(const QString &jobId, QString *resultOut,
                  QString *errorOut);

  AssistantProfile *profile() const { return m_profile; }
  AssistantMemory *memory() const { return m_memory; }
  AssistantActivity *activity() const { return m_activity; }
  SpeechAnimator *animator() const { return m_animator; }
  assistant::AssistantToolRegistry *tools() const { return m_tools; }

signals:
  void assistantSaid(const QString &text);

  void assistantReplyStarted(const QString &nodeId);
  void assistantChunk(const QString &nodeId, const QString &text);
  void assistantTurnFinished(const QString &nodeId);

  void statusMessage(const QString &text);
  void statusChanged(const QString &status);

  void jobCreated(const QString &jobId, const QString &nodeId,
                  ChatNode::Kind kind, const QString &title,
                  const QString &detail);

  void jobStateChanged(const QString &jobId, ChatNode::State state);
  void jobCompleted(const QString &jobId, const QString &result);
  void jobFailed(const QString &jobId, const QString &error);

private slots:
  void onLlmDelta(const QUuid &token, const QString &text);
  void onLlmFinished(const QUuid &token);
  void onLlmToolCalls(const QUuid &token, const QJsonArray &toolCalls);
  void onLlmError(const QUuid &token, const QString &error);

  void onOverseerRequestFinished(const QString &sessionName,
                                 const QString &requestId, bool ok,
                                 const QString &summary,
                                 const QString &filePath);

  void onSearchJobFinished(const QString &jobId, const QString &result);
  void onSearchJobFailed(const QString &jobId, const QString &reason);

private:
  QString startSearchJob(const QString &query, const QString &nodeId);
  QString startNoteEditJob(const QString &notePath,
                           const QString &instruction,
                           const QString &nodeId);

  void applyJobCompletion(const Job &job);

  QJsonArray buildMessages(const QString &userText);
  assistant::AssistantToolContext buildToolContext();
  void runToolRound(const QJsonArray &toolCalls,
                    const QJsonArray &priorMessages);
  void finishTurn();
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
  int m_toolRoundsRemaining = 8;

  QString m_turnReplyBuffer;
  QString m_lastReply;
  QString m_activeReplyNode;
  QString m_currentViewNodeId;
  CompletionPolicy m_completionPolicy = CompletionPolicy::Automatic;

  QHash<QString, Job> m_jobs;
  quint64 m_nextJobOrdinal = 1;

  QHash<QString, RetrievalLoop *> m_searchLoops;
  QHash<QString, QString> m_searchBuffers;

  QHash<QString, NoteEditJob *> m_noteEditJobs;

  QHash<QString, CompletionPolicy> m_jobPolicies;

  QStringList m_pendingForPrompt;
  QStringList m_pendingForUserMessage;
};