#pragma once

#include "../agent/Tool.h"
#include "ConductorQueue.h"
#include "ConductorRoster.h"
#include "ConductorTypes.h"
#include "DependencyGraph.h"
#include "SessionSettings.h"
#include "ToolRegistry.h"
#include "TranscriptEvent.h"

#include "inference/InferenceService.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

class AutomationStrip;
class FileAgent;
class OverseerSession;
class PayloadLogger;
class TranscriptStore;
class TextDocument;
class TextEdit;
class EditPlanner;
class EditSession;
class EditCommand;
class Workstation;

// One session's conductor. Owns the queue, the roster, the dependency
// graph, the transcript store, the file agents, the scoped edit
// sessions, the session's memory proposals, and the session's logger.
//
// A runner is created by OverseerSessionManager when a session is
// opened, and lives for the lifetime of the application. Runners are
// independent: two sessions can run requests at the same time, each
// with its own conductor, its own agents, and its own transcript.
//
// The runner has no view. It emits signals and the view binds to them.
class OverseerRunner : public QObject {
  Q_OBJECT

public:
  // A single thing the user needs to act on. Built by pendingActions()
  // and rendered in the side panel's "User actions" tab. There is a
  // one-to-one correspondence between a PendingAction and an item in
  // the runner's source-of-truth lists (m_proposals, m_scopedSessions).
  struct PendingAction {
    enum class Kind {
      MemoryProposal,
      EditPlan,
    };

    // How the memory proposal card should present itself. Ignored
    // when kind == EditPlan.
    enum class ProposalMode {
      NewFact,
      Replace,
      Delete,
    };

    Kind kind = Kind::MemoryProposal;
    ProposalMode proposalMode = ProposalMode::NewFact;

    // proposal key or planId. Unique within the runner.
    QString key;

    // Memory proposals: the fact (empty for Delete). Edit plans: the
    // file name.
    QString title;

    // Memory proposals: the rationale. Edit plans: an "N edits" line.
    QString subtitle;

    // Memory proposals: the fact being replaced or deleted. Empty for
    // NewFact. Edit plans: unused.
    QString replacedFact;

    // The session name, for routing from an OS notification.
    QString sessionName;

    // Memory proposals only: "global" or "session".
    QString scope;
  };

  OverseerRunner(InferenceService *inferenceService,
                 const QString &sessionName,
                 QObject *parent = nullptr);

  ~OverseerRunner() override;

  void setWorkstation(Workstation *workstation);
  void setToolCallDepthLimit(int limit);

  QString sessionName() const { return m_sessionName; }
  OverseerSession *session() const { return m_session; }

  ConductorQueue *queue() const { return m_queue; }
  ConductorRoster *roster() const { return m_roster; }
  DependencyGraph *dependencies() { return &m_dependencies; }
  TranscriptStore *transcriptStore() const { return m_transcriptStore; }
  SessionSettings settings() const { return m_sessionSettings; }

  FileAgent *fileAgentById(const QString &id) const;

  // The current set of pending actions in this session: memory
  // proposals with status "pending" and edit plans awaiting review.
  // Read-only projection of the runner's source-of-truth lists.
  QList<PendingAction> pendingActions() const;

  // Focused editor and document, set by the view as the user works.
  // Used by scoped edits and by agents that need the current cursor.
  void setFocusedFilePath(const QString &absolutePath);
  void setFocusedDocument(TextDocument *document, TextEdit *editor);

  // Submit a request on the user's behalf. Returns the request id, or
  // an empty string when the session is not open.
  QString submitRequest(const QString &text);

  // Submit a request on the assistant's behalf. Identical to
  // submitRequest except the transcript marks the request origin as
  // Lore.
  QString submitRequestFromLore(const QString &text);

  void cancelRequest(const QString &requestId);
  void removeFailedRequest(const QString &requestId);

  // Automation settings for this session. Persisted to the session's
  // settings.json.
  void setSessionSettings(const SessionSettings &settings);
  // Memory proposals. The view calls these when the user accepts or
  // rejects a proposal card.
  void acceptProposal(const QString &key, const QString &scope);
  void rejectProposal(const QString &key);
  // Interactive plan card. The view forwards to these when the user
  // accepts, rejects, applies, or cancels edits in an edit plan.
  void acceptPlanEdit(const QString &planId, int editId);
  void rejectPlanEdit(const QString &planId, int editId);
  void applyPlan(const QString &planId);
  void cancelPlan(const QString &planId);
  void reloadMemoryPanels();

signals:
  // Emitted when a request reaches a terminal state. The manager
  // forwards this to whoever is interested, including LoreAssistant.
  void requestFinished(const QString &sessionName,
                       const QString &requestId, bool ok,
                       const QString &summary, const QString &filePath);

  // Emitted when this runner wants the view to open a file, focus a
  // window, or save a workstation file.
  void fileWritten(const QString &absolutePath);
  void fileOpenRequested(const QString &absolutePath);
  void fileCloseRequested(const QString &absolutePath);
  void saveWorkstationFileRequested(const QString &absolutePath);

  void planGenerationStarted(const QString &absolutePath);
  void planReviewReady(const QString &absolutePath);
  void planApplied(const QString &absolutePath);
  void planFailed(const QString &absolutePath);

  // Emitted when a file agent hits its per-agent tool call depth
  // limit. The view surfaces this to the user.
  void agentDepthLimitReached(const QString &agentId, int limit);

  // Emitted when the queue, the roster, the transcript, the memory
  // proposals, or the pending action set change. The view listens and
  // refreshes.
  void changed();

public slots:
  void drainQueue();

private:
  struct MemoryProposal {
    QString key;
    QString fact;              // empty for Delete
    QString rationale;
    QString status;            // "pending", "accepted", "rejected"
    QString scope;             // "global" or "session"

    // If non-empty, this proposal replaces or deletes the fact whose
    // key matches. Empty for a new-fact proposal. `replacedFact` is
    // the verbatim text of the fact being superseded, for display.
    QString replaces;
    QString replacedFact;

    QString acceptedScope;     // set on accept
  };

  struct ScopedSession {
    QString planId;
    QString requestId;
    QString filePath;
    QString instruction;
    QVector<EditCommand> commands;

    EditPlanner *planner = nullptr;
    EditSession *session = nullptr;

    // True when the plan has finished generating and is waiting for
    // the user to apply or cancel. Cleared on apply, cancel, and
    // teardown. Drives the "User actions" tab.
    bool awaitingReview = false;

    QString originAgentId;
    QString originTaskId;
  };

  struct DispatchPlan {
    enum class Kind {
      Answer,
      Reject,
      Defer,
      Redirect,
      Route,
      SpawnEdit,
      SpawnAgent,
      ProposeMemory,
    };

    Kind kind = Kind::Route;

    QString agentId;
    QString domain;
    QString filePath;
    QString instruction;
    QString claimedPath;
    QString answer;
    QString reason;
    QStringList dependencies;

    // ProposeMemory payload.
    QString memoryFact;        // empty means delete
    QString memoryRationale;
    QString memoryScope;
    QString memoryReplaces;    // key of the fact to replace or delete
  };

  void openSession();
  void closeSession();

  void appendEvent(const TranscriptEvent &event);

  QString buildConductorPrompt(const ConductorRequest &request) const;

  void routeRequest(const ConductorRequest &request);

  void applyRoutingDecision(const QString &requestId,
                            const QJsonObject &decision);

  DispatchPlan decideDispatch(const QString &requestId,
                              const QJsonObject &decision) const;

  void recordEdges(const QString &requestId,
                   const QStringList &dependencies);

  bool dependenciesSatisfied(const QString &requestId) const;

  bool dependenciesBlocked(const QString &requestId) const;

  void failDependentsOf(const QString &requestId, const QString &reason);

  // Called after a request reaches a terminal state. Fails any
  // dependent whose dependency has now failed or been rejected, and
  // re-evaluates deferrals.
  void failBlockedDependentsAfterTerminal(const QString &requestId);

  // Mark every non-terminal request whose dependency has failed or
  // been rejected as failed, and mark every other non-terminal
  // request with an unmet dependency as deferred. Called by
  // drainQueue() before it picks a candidate, and by the terminal
  // path.
  void settleDependentRequests();

  int expertiseForInstruction(const QString &instruction,
                              const QString &agentId) const;

  FileAgent *bestExpertForInstruction(const QString &instruction) const;

  FileAgent *leastLoadedAgent() const;

  QStringList pathsNamedByInstruction(const QString &instruction) const;

  bool instructionTouchesClaim(const QString &instruction,
                               QString *claimedPath) const;

  void onFileWriteClaimed(const QString &agentId, const QString &taskId,
                          const QString &relativePath);
  void onFileWriteReleased(const QString &agentId, const QString &taskId,
                           const QString &relativePath);

  QString writeOwnerForPath(const QString &relativePath) const;

  QString claimedPathForInstruction(const QString &instruction) const;

  bool instructionCollidesWithAgentClaims(const QString &instruction,
                                          const QString &agentId) const;

  void handleSpawnScopedEdit(const QString &requestId,
                             const QString &filePath,
                             const QString &instruction,
                             const QString &originAgentId = QString(),
                             const QString &originTaskId = QString());

  void onPlannerValidated(const QString &planId,
                          const QVector<EditCommand> &commands);
  void onPlannerFailed(const QString &planId, const QString &reason);

  void tearDownScopedSession(const QString &planId);

  void buildEditPlanEvent(const ScopedSession &ctx);

  void autoApplySession(const QString &planId);

  void appendActionSummary(const QString &line);

  void syncRoster();

  QString spawnFileAgent(const QString &domain);

  // Record a memory proposal. If `replacesKey` is non-empty, the
  // proposal supersedes an existing fact whose key matches. `fact`
  // may be empty only when `replacesKey` is non-empty; that means the
  // proposal is a deletion. Returns the new proposal's key, or an
  // empty string on failure.
  QString recordProposal(const QString &fact, const QString &rationale,
                         const QString &scope,
                         const QString &replacesKey = QString());

  void setProposalStatus(const QString &key, const QString &status,
                         const QString &acceptedScope);

  // Deterministic key for a fact within a scope. Stable across
  // rationale edits. Replaces the old key which incorporated the
  // rationale.
  static QString factKey(const QString &fact, const QString &scope);

  // Resolve a proposal key to the fact text it names. Returns empty
  // if the key is not in m_proposals or the proposal is a deletion.
  QString factForKey(const QString &key) const;

  QString proposalsSidecarPath() const;

  void loadProposals();
  void saveProposals();

  PayloadLogger *sessionLogger() const;

  void announceRequestFinished(const QString &requestId, bool ok,
                               const QString &summary,
                               const QString &filePath = QString());

  InferenceService *m_inferenceService = nullptr;
  QString m_sessionName;
  OverseerSession *m_session = nullptr;

  ConductorQueue *m_queue = nullptr;
  ConductorRoster *m_roster = nullptr;
  DependencyGraph m_dependencies;

  TranscriptStore *m_transcriptStore = nullptr;

  SessionSettings m_sessionSettings;

  PayloadLogger *m_payloadLogger = nullptr;
  PayloadLogger *m_sessionLogger = nullptr;

  ToolRegistry m_tools;

  InferenceService::RequestToken m_activeConductorToken;
  QString m_activeRequestId;
  QString m_conductorRawText;
  QString m_routingRequestId;

  int m_toolCallDepthLimit = 16;
  int m_nextAgentOrdinal = 1;

  QList<MemoryProposal> m_proposals;
  QStringList m_actionSummary;

  QString m_focusedFilePath;
  TextDocument *m_focusedDocument = nullptr;
  TextEdit *m_focusedEditor = nullptr;

  Workstation *m_workstation = nullptr;

  QHash<QString, ScopedSession> m_scopedSessions;
  QHash<QString, FileAgent *> m_fileAgents;
  QHash<QString, QString> m_taskToAgent;
  QHash<QString, QString> m_writeOwner;
  QHash<QString, QStringList> m_agentWritePaths;
};