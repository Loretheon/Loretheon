#pragma once

#include "ConductorQueue.h"
#include "ConductorRoster.h"
#include "DependencyGraph.h"
#include "OverseerTool.h"
#include "OverseerToolRegistry.h"
#include "SessionSettings.h"
#include "ThemeAware.h"
#include "TranscriptEvent.h"

#include "inference/InferenceService.h"

#include <QHash>
#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>
#include <QVector>
#include <QWidget>

#include "NotificationService.h"

class AutomationStrip;
class FileAgent;
class MemoryProposalCard;
class OverseerSession;
class OverseerSessionList;
class OverseerSidePanel;
class PayloadLogger;
class TranscriptPanel;
class TranscriptStore;
class TextDocument;
class TextEdit;
class EditPlanner;
class EditSession;
class EditCommand;
class Workstation;

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QSpinBox;
class QSplitter;
class QStackedWidget;
class QTabWidget;
class QTextEdit;
class QUrl;
class QVBoxLayout;

class OverseerWidget : public QWidget, public ThemeAware {
  Q_OBJECT

public:
  explicit OverseerWidget(InferenceService *inferenceService,
                          QWidget *parent = nullptr);

  ~OverseerWidget() override;

  void setThemeTokens(const ThemeTokens &tokens) override;

  void setWorkstation(Workstation *workstation);

  OverseerSessionList *sessionListPanel() const { return m_sessionListPanel; }
  OverseerSidePanel *sidePanel() const { return m_sidePanel; }
  TranscriptPanel *transcriptPanel() const { return m_transcriptPanel; }
  TranscriptStore *transcriptStore() const { return m_transcriptStore; }

  ConductorQueue *queue() const { return m_queue; }
  ConductorRoster *roster() const { return m_roster; }
  DependencyGraph *dependencies() { return &m_dependencies; }

  OverseerSession *currentSession() const { return m_currentSession; }

  int activeSessionCount() const { return m_scopedSessions.size(); }

  // Spawn a new file agent. Does not check the cap: the caller
  // (applyRoutingDecision) checks the cap before calling.
  QString spawnFileAgent(const QString &domain);
  FileAgent *fileAgentById(const QString &id) const;

public slots:
  void setFocusedFilePath(const QString &absolutePath);
  void setFocusedDocument(TextDocument *document, TextEdit *editor);

signals:
  void fileWritten(const QString &absolutePath);
  void fileOpenRequested(const QString &absolutePath);
  void fileCloseRequested(const QString &absolutePath);

  void planGenerationStarted(const QString &absolutePath);
  void planReviewReady(const QString &absolutePath);
  void planApplied(const QString &absolutePath);
  void planFailed(const QString &absolutePath);

  void saveWorkstationFileRequested(const QString &absolutePath);

public slots:
  void addOverviewReference(const QString &path);
  void addOverviewReferences(const QStringList &paths);

  void openSessionByName(const QString &name);

  void submitRequest(const QString &text);
  void cancelRequest(const QString &requestId);

  void removeFailedRequest(const QString &requestId);

private slots:
  void onSessionSelected(const QString &name);
  void onSessionCleared();
  void onNewSessionRequested();
  void onToolCallDepthChanged(int value);

  void onProposalAccepted(const QString &key, const QString &scope);
  void onProposalRejected(const QString &key);

  void onPlanEditAccepted(const QString &planId, int editId);
  void onPlanEditRejected(const QString &planId, int editId);
  void onPlanApplyRequested(const QString &planId);
  void onPlanCancelRequested(const QString &planId);

  void onAutomationSettingsChanged(const SessionSettings &settings);

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

  void rebuildSessionList();
  void openSession(OverseerSession *session);
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

  // Mark every unfinished direct dependent of `requestId` as failed,
  // with a reason naming the failed dependency. Called when a task
  // reaches terminal failure (retry exhausted). Prevents a
  // permanently failed request from holding its dependents in the
  // inbox forever.
  void failDependentsOf(const QString &requestId, const QString &reason);

  // How many of an agent's filesSeen entries the instruction names.
  // Higher means the agent has more track record on the files this
  // request needs, and is therefore the better expert.
  int expertiseForInstruction(const QString &instruction,
                              const QString &agentId) const;

  // The agent with the highest expertise score on the instruction,
  // ties broken by least queue depth then lowest id. Returns nullptr
  // if no agent has any expertise match or there are no agents.
  FileAgent *bestExpertForInstruction(const QString &instruction) const;

  // The agent with the shallowest queue, ties broken by lowest id.
  // Used when a request names no file any agent has seen.
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

  void reloadMemoryPanels();

  void appendActionSummary(const QString &line);

  void syncRoster();

  QString recordProposal(const QString &fact, const QString &rationale,
                         const QString &scope);

  void setProposalStatus(const QString &key, const QString &status,
                         const QString &acceptedScope);

  QString proposalsSidecarPath() const;

  void loadProposals();
  void saveProposals();

  PayloadLogger *sessionLogger() const;

  QStringList m_actionSummary;

  InferenceService *m_inferenceService = nullptr;

  OverseerSessionList *m_sessionListPanel = nullptr;
  OverseerSidePanel *m_sidePanel = nullptr;
  TranscriptPanel *m_transcriptPanel = nullptr;

  TranscriptStore *m_transcriptStore = nullptr;

  ConductorQueue *m_queue = nullptr;
  ConductorRoster *m_roster = nullptr;

  DependencyGraph m_dependencies;

  AutomationStrip *m_automationStrip = nullptr;
  SessionSettings m_sessionSettings;

  PayloadLogger *m_payloadLogger = nullptr;
  PayloadLogger *m_sessionLogger = nullptr;

  QSpinBox *m_toolCallDepthSpin = nullptr;

  QLineEdit *m_input = nullptr;
  QPushButton *m_sendButton = nullptr;

  OverseerSession *m_currentSession = nullptr;

  QLabel *m_sessionHeader = nullptr;

  OverseerToolRegistry m_tools;

  InferenceService::RequestToken m_activeConductorToken;
  QString m_activeRequestId;
  QString m_conductorRawText;

  QString m_routingRequestId;

  int m_toolCallDepthLimit = 16;

  int m_nextAgentOrdinal = 1;

  QList<MemoryProposal> m_proposals;

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