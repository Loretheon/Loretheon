#pragma once

#include "AssistantToolRegistry.h"
#include "ChatNode.h"
#include "ChatTreeStore.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QQueue>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QUuid>

namespace assistant {
class AssistantToolRegistry;
}
class AssistantActivity;
class AssistantMemory;
class AssistantProfile;
class AssistantToolRegistry;
class AvatarWidget;
class ChatTree;
class DocumentArea;
class DocumentManager;
class InferenceService;
class IngestService;
class MemoryIndex;
class NoteEditJob;
class NotePromoter;
class OverseerSessionManager;
class ProfileEditJob;
class RetrievalLoop;
class ScopeIndex;
class SearchService;
class SpeechAnimator;

class LoreAssistant : public QObject {
  Q_OBJECT

public:
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

  struct Config {
    InferenceService *inference = nullptr;
    AvatarWidget *avatar = nullptr;
    DocumentManager *documents = nullptr;
    DocumentArea *documentArea = nullptr;
    SearchService *search = nullptr;
    OverseerSessionManager *overseerManager = nullptr;
    NotePromoter *promoter = nullptr;
    ScopeIndex *scopeIndex = nullptr;
    IngestService *ingest = nullptr;

    QString notesRoot;
    QString root;
  };

  explicit LoreAssistant(const Config &config,
                         QObject *parent = nullptr);
  ~LoreAssistant() override;

  bool start();
  void stop();

  void say(const QString &text);

  // Begin a turn. Multiple turns may be active at once. Each turn has
  // its own token, its own reply node, its own messages array, and its
  // own tool-round budget. The turn ends when the model stops calling
  // tools, the budget is exhausted, the token errors, or the watchdog
  // fires.
  void handleUserMessage(const QString &text);

  // Rebuild the recent-tail buffer from a loaded chat tree. The shell
  // calls this after it has loaded a segment into the tree, and on
  // segment switch. Passing nullptr clears the tail.
  void setCurrentChat(const ChatTree *tree);

  // Import a file into the assistant's memory tree. The file is run
  // through IngestService, which converts it to markdown, and the
  // result is written under <root>/memories/topics/. On success, the
  // memory index is refreshed for the new file and a status message is
  // emitted with the file's name.
  //
  // Returns false and emits statusMessage if the ingest service is
  // not available or the path cannot be handled by any extractor.
  bool importToMemory(const QString &sourcePath, const QString &topicName);


  // True if the ingest service is available and has an extractor
  // registered for the given path. Used by the mind map drop handler
  // to decide whether a dropped file is acceptable.
  bool canImport(const QString &sourcePath) const;

  
  bool isTurnActive() const { return !m_turns.isEmpty(); }
  int activeTurnCount() const { return m_turns.size(); }

  QString lastReply() const { return m_lastReply; }
  QString rootPath() const { return m_config.root; }

  const Job *job(const QString &jobId) const;
  QStringList jobIds() const;

  void abortJob(const QString &jobId);

  // Abort every active turn and every job. Stops all in-flight work.
  void abortAll();

  // Abort one turn by the id of its reply node. The turn's background
  // jobs are left running; only the model request is cancelled.
  void abortTurn(const QString &replyNodeId);

  bool waitForJob(const QString &jobId, QString *resultOut,
                  QString *errorOut);

  AssistantProfile *profile() const { return m_profile; }
  AssistantMemory *memory() const { return m_memory; }
  AssistantActivity *activity() const { return m_activity; }
  SpeechAnimator *animator() const { return m_animator; }
  assistant::AssistantToolRegistry *tools() const { return m_tools; }
  ChatTreeStore *chatStore() const { return m_chatStore; }

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

  void knowledgeChanged();

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

  void onTurnWatchdog();

private:
  struct Turn {
    QUuid token;
    QString replyNode;
    QJsonArray messages;
    int toolRoundsRemaining = 8;
    QString replyBuffer;
    bool finished = false;
  };

  QString startSearchJob(const QString &query, const QString &nodeId);
  QString startNoteEditJob(const QString &notePath,
                           const QString &instruction,
                           const QString &nodeId);
  QString startProfileEditJob(const QString &path,
                              const QString &instruction,
                              const QString &nodeId);

  QJsonArray buildMessages(const QString &userText);
  assistant::AssistantToolContext buildToolContext();

  void runToolRound(const QUuid &turnToken,
                    const QJsonArray &toolCalls,
                    const QJsonArray &priorMessages);

  void sendTurnRequest(Turn &turn);

  void finishTurn(const QUuid &turnToken);
  void abandonTurn(const QUuid &turnToken);

  QString systemPrompt() const;

  QString writePaste(const QString &body);
  QString pastePath(const QString &id) const;

  void recordRecentTurn(const QString &userText,
                        const QString &assistantText);
  void trimRecentTail();

  Config m_config;

  AssistantProfile *m_profile = nullptr;
  AssistantMemory *m_memory = nullptr;
  MemoryIndex *m_memoryIndex = nullptr;
  AssistantActivity *m_activity = nullptr;
  assistant::AssistantToolRegistry *m_tools = nullptr;
  SpeechAnimator *m_animator = nullptr;
  ChatTreeStore *m_chatStore = nullptr;

  bool m_started = false;

  QHash<QUuid, Turn> m_turns;
  QHash<QString, QUuid> m_nodeToTurn;

  QHash<QUuid, QString> m_turnUserText;

  QJsonArray m_recentTail;

  QString m_lastReply;

  QHash<QString, Job> m_jobs;
  quint64 m_nextJobOrdinal = 1;

  QHash<QString, RetrievalLoop *> m_searchLoops;
  QHash<QString, QString> m_searchBuffers;

  QHash<QString, NoteEditJob *> m_noteEditJobs;
  QHash<QString, ProfileEditJob *> m_profileEditJobs;

  QStringList m_pendingForPrompt;
  QStringList m_pendingForUserMessage;

  QTimer *m_watchdog = nullptr;
};