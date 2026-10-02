#pragma once

#include "ConductorRoster.h"
#include "inference/InferenceService.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

class InferenceService;
class PayloadLogger;
class OverseerSession;

// The single memory owner for a session. Owns global and session
// memory files, and the proposals sidecar. Every memory mutation goes
// through here, whether the trigger is the conductor's propose_memory
// action or the user clicking accept on a proposal card.
//
// A task carries either a natural-language instruction (the agent runs
// its LLM to decide what to do) or a pre-decided action object (the
// agent applies it directly without an LLM round-trip). Pre-decided
// tasks come from the conductor's propose_memory plans, which are
// already structured; there is no reason to ask a model to translate
// them back into the agent's own action vocabulary.
class MemoryAgent : public QObject {
  Q_OBJECT

public:
  struct Task {
    QString id;
    QString requestId;

    // Free-form instruction for the LLM. Ignored when
    // preDecidedAction is non-empty.
    QString instruction;

    // A structured action of the form
    //   {"fact": "...", "rationale": "...", "scope": "global"|"session"}
    // or
    //   {"fact": "...", "rationale": "...", "scope": "...",
    //    "replaces": "<key>"}
    // or
    //   {"fact": "", "scope": "...", "replaces": "<key>"}
    // When non-empty, the agent applies it without consulting the LLM.
    QJsonObject preDecidedAction;
  };

  struct Proposal {
    QString key;
    QString fact;
    QString rationale;
    QString status;         // "pending", "accepted", "rejected", "failed"
    QString scope;          // "global" or "session"
    QString replaces;       // key of the fact being replaced or deleted
    QString replacedFact;   // display text of the target fact
    QString acceptedScope;
    QString fallbackNote;
    QString requestId;      // originating request
  };

  MemoryAgent(const QString &id,
              OverseerSession *session,
              InferenceService *inferenceService,
              PayloadLogger *logger,
              QObject *parent = nullptr);

  ~MemoryAgent() override;

  QString id() const { return m_id; }

  bool isBusy() const { return m_busy; }
  int queueDepth() const { return m_queue.size(); }
  QString activeTaskId() const { return m_current.id; }

  QVector<Task> queue() const { return m_queue; }

  QVector<Proposal> proposals() const { return m_proposals; }

  ConductorWorker rosterEntry() const;
  QString summaryForConductor() const;

  void setToolCallDepthLimit(int limit);
  int toolCallDepthLimit() const { return m_toolCallDepthLimit; }
  int toolCallCount() const { return m_toolCallCount; }

  void enqueue(const Task &task);
  bool cancel(const QString &taskId);
  void finishTask(const QString &taskId, bool ok,
                  const QString &resultOrError);

  bool acceptProposal(const QString &key, const QString &scope);
  bool rejectProposal(const QString &key);

  void load();
  void save() const;

  QString memoryPathForScope(const QString &scope) const;

signals:
  void taskFinished(const QString &taskId, bool ok,
                    const QString &resultOrError);

  void proposalsChanged();

  void stateChanged();

  void depthLimitReached(const QString &agentId, int limit);

private slots:
  void onResponseFinished(const InferenceService::RequestToken &token);
  void onResponseDelta(const InferenceService::RequestToken &token,
                       const QString &text);
  void onResponseError(const InferenceService::RequestToken &token,
                       const QString &error);

private:
  void beginNextTask();
  void dispatchTurn();
  void scheduleDispatch();

  // Apply a pre-decided action without consulting the LLM.
  void applyPreDecided(const QJsonObject &action);

  // Route a response from the LLM. Used only for instruction tasks.
  void applyAction(const QJsonObject &action);

  void finishCurrent(bool ok, const QString &resultOrError);

  QString buildPrompt(const Task &task) const;

  void logAgent(const QString &tag, const QString &content);

  bool isCurrentToken(const InferenceService::RequestToken &token) const;

  QString proposalsPath() const;

  QStringList factsForScope(const QString &scope) const;

  bool applyFactAdd(const QString &scope, const QString &fact,
                    QString *error);
  bool applyFactReplace(const QString &scope, const QString &replacedFact,
                        const QString &newFact, QString *fallbackNote,
                        QString *error);
  bool applyFactDelete(const QString &scope, const QString &replacedFact,
                       QString *error);

  QString m_id;
  OverseerSession *m_session = nullptr;

  InferenceService *m_inferenceService = nullptr;
  PayloadLogger *m_logger = nullptr;

  QVector<Task> m_queue;
  Task m_current;
  bool m_busy = false;

  InferenceService::RequestToken m_activeToken;
  bool m_pendingDispatch = false;
  bool m_hasActiveToken = false;

  QString m_activeTaskId;
  QString m_accumulated;

  QVector<QJsonObject> m_taskToolCalls;

  QVector<Proposal> m_proposals;

  int m_toolCallDepthLimit = 64;
  int m_toolCallCount = 0;

  static constexpr int kMaxParseRetries = 1;
};