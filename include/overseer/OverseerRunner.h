#pragma once

#include "../agent/Tool.h"
#include "ConductorQueue.h"
#include "ConductorRoster.h"
#include "ConductorTypes.h"
#include "DependencyGraph.h"
#include "SessionSettings.h"
#include "TextDocument.h"
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
class MemoryAgent;
class OverseerSession;
class PayloadLogger;
class TranscriptStore;
class TextDocument;
class TextEdit;
class EditPlanner;
class EditSession;
class EditCommand;
class Workstation;

class OverseerRunner : public QObject {
  Q_OBJECT

public:
  struct PendingAction {
    enum class Kind {
      MemoryProposal,
      EditPlan,
    };

    enum class ProposalMode {
      NewFact,
      Replace,
      Delete,
    };

    Kind kind = Kind::MemoryProposal;
    ProposalMode proposalMode = ProposalMode::NewFact;

    QString key;
    QString title;
    QString subtitle;
    QString replacedFact;
    QString fallbackNote;
    QString sessionName;
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
  MemoryAgent *memoryAgent() const { return m_memoryAgent; }

  QList<PendingAction> pendingActions() const;

  void setFocusedFilePath(const QString &absolutePath);
  void setFocusedDocument(TextDocument *document, TextEdit *editor);

  QString submitRequest(const QString &text);
  QString submitRequestFromLore(const QString &text);

  void cancelRequest(const QString &requestId);
  void removeFailedRequest(const QString &requestId);
  void retryFailedRequest(const QString &requestId);
  void skipFailedRequest(const QString &requestId);

  void setSessionSettings(const SessionSettings &settings);
  void acceptProposal(const QString &key, const QString &scope);
  void rejectProposal(const QString &key);
  void acceptPlanEdit(const QString &planId, int editId);
  void rejectPlanEdit(const QString &planId, int editId);
  void applyPlan(const QString &planId);
  void cancelPlan(const QString &planId);
  void reloadMemoryPanels();

signals:
  void requestFinished(const QString &sessionName,
                       const QString &requestId, bool ok,
                       const QString &summary, const QString &filePath);

  void fileWritten(const QString &absolutePath);
  void fileOpenRequested(const QString &absolutePath);
  void fileCloseRequested(const QString &absolutePath);
  void saveWorkstationFileRequested(const QString &absolutePath);

  void planGenerationStarted(const QString &absolutePath);
  void planReviewReady(const QString &absolutePath);
  void planApplied(const QString &absolutePath);
  void planFailed(const QString &absolutePath);

  void agentDepthLimitReached(const QString &agentId, int limit);

  void changed();

public slots:
  void drainQueue();

private:
  struct ScopedSession {
    QString planId;
    QString requestId;
    QString filePath;
    QString instruction;
    QVector<EditCommand> commands;

    EditPlanner *planner = nullptr;
    EditSession *session = nullptr;

    bool awaitingReview = false;

    QString originAgentId;
    QString originTaskId;

    // Owned by the runner. Freed in tearDownScopedSession. Used as the
    // planning and apply target for the scoped edit. Null until the
    // runner has loaded the file.
    TextDocument *document = nullptr;
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
      FanOut,
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

    QString memoryFact;
    QString memoryRationale;
    QString memoryScope;
    QString memoryReplaces;

    QVector<QJsonObject> fanOutActions;
    QVector<QJsonObject> order;
  };

  void openSession();
  void closeSession();

  void appendEvent(const TranscriptEvent &event);

  QString buildConductorPrompt(const ConductorRequest &request) const;

  void routeRequest(const ConductorRequest &request);

  void applyRoutingDecision(const QString &requestId,
                            const QJsonObject &decision);

  void applySinglePlan(const QString &requestId, const DispatchPlan &plan);

  DispatchPlan decideDispatch(const QString &requestId,
                              const QJsonObject &decision) const;

  DispatchPlan parseAction(const QString &requestId,
                           const QJsonObject &action) const;

  QString displayLabelForAction(const QJsonObject &action) const;
  
  static bool isPreDecidedAction(const QString &text);

  void recordEdges(const QString &requestId,
                   const QStringList &dependencies);

  bool dependenciesSatisfied(const QString &requestId) const;

  bool dependenciesBlocked(const QString &requestId) const;

  QStringList unsatisfiedDependencies(const QString &requestId) const;

  // Pause every dependent of requestId. Used in place of the old
  // fail-everything cascade: a failed request leaves its dependents
  // deferred in the inbox until the user retries, skips, or removes
  // the failure.
  void pauseDependentsOf(const QString &requestId, const QString &reason);

  void failBlockedDependentsAfterTerminal(const QString &requestId);

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

  void tearDownScopedSession(const QString &planId,
                           bool persistDocument = false);

  void buildEditPlanEvent(const ScopedSession &ctx);

  void autoApplySession(const QString &planId);

  void appendActionSummary(const QString &line);

  void syncRoster();

  QString spawnFileAgent(const QString &domain);

  void spawnMemoryAgent();

  void refreshFileAgentMemory();

  void onMemoryAgentTaskFinished(const QString &taskId, bool ok,
                                 const QString &summary);

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

  bool m_conductorRetryInFlight = false;
  QString m_conductorRetryRaw;

  int m_toolCallDepthLimit = 16;
  int m_nextAgentOrdinal = 1;

  QStringList m_actionSummary;

  QString m_focusedFilePath;
  TextDocument *m_focusedDocument = nullptr;
  TextEdit *m_focusedEditor = nullptr;

  Workstation *m_workstation = nullptr;

  MemoryAgent *m_memoryAgent = nullptr;

  QHash<QString, ScopedSession> m_scopedSessions;
  QHash<QString, FileAgent *> m_fileAgents;
  QHash<QString, QString> m_taskToAgent;
  QHash<QString, QString> m_retryWorker;
  QHash<QString, QString> m_writeOwner;
  QHash<QString, QStringList> m_agentWritePaths;
};