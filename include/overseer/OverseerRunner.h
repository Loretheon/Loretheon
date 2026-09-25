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

  // Emitted when the queue, the roster, the transcript, or the memory
  // proposals change. The view listens and refreshes.
  void changed();

public slots:
  void drainQueue();

private:
  struct MemoryProposal {
    QString key;
    QString fact;
    QString rationale;
    QString status;
    QString scope;
    QString acceptedScope;
  };

  struct ScopedSession {
    QString planId;
    QString requestId;
    QString filePath;
    QString instruction;
    QVector<EditCommand> commands;

    EditPlanner *planner = nullptr;
    EditSession *session = nullptr;

    bool awaitingAutoApply = false;

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

  QString recordProposal(const QString &fact, const QString &rationale,
                         const QString &scope);

  void setProposalStatus(const QString &key, const QString &status,
                         const QString &acceptedScope);

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