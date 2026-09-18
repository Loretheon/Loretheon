#pragma once

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

  OverseerSession *currentSession() const { return m_currentSession; }

  int activeSessionCount() const { return m_scopedSessions.size(); }

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

private slots:
  void onSessionSelected(const QString &name);
  void onSessionCleared();
  void onSendClicked();
  void onNewSessionRequested();
  void onToolCallDepthChanged(int value);

  void onProposalAccepted(const QString &key, const QString &scope);
  void onProposalRejected(const QString &key);

  void startScopedEdit(TextEdit *editor, TextDocument *document,
                       const QString &instruction);
  void onPlannerValidated(const QString &planId,
                          const QVector<EditCommand> &commands);
  void onPlannerFailed(const QString &planId, const QString &reason);

  void onPlanEditAccepted(const QString &planId, int editId);
  void onPlanEditRejected(const QString &planId, int editId);
  void onPlanApplyRequested(const QString &planId);
  void onPlanCancelRequested(const QString &planId);

  void onAutomationSettingsChanged(const SessionSettings &settings);

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
    QString filePath;
    QString instruction;
    QVector<EditCommand> commands;

    EditPlanner *planner = nullptr;
    EditSession *session = nullptr;

    bool awaitingAutoApply = false;
  };

  void rebuildSessionList();
  void openSession(OverseerSession *session);
  void closeSession();

  void appendEvent(const TranscriptEvent &event);

  QString buildSystemPrompt() const;

  void dispatchChatRequest();

  void handleToolCalls(const QJsonArray &toolCalls);
  void executeToolCalls(const QJsonArray &toolCalls);

  OverseerTool::Context currentToolContext() const;

  QString recordProposal(const QString &fact, const QString &rationale,
                         const QString &scope);
  void setProposalStatus(const QString &key, const QString &status,
                         const QString &acceptedScope);
  QString proposalsSidecarPath() const;
  void loadProposals();
  void saveProposals();

  void tearDownScopedSession(const QString &planId);
  void tearDownAllScopedSessions();

  void buildEditPlanEvent(const ScopedSession &ctx);

  void autoApplySession(const QString &planId);

  void reloadMemoryPanels();

  InferenceService *m_inferenceService = nullptr;

  OverseerSessionList *m_sessionListPanel = nullptr;
  OverseerSidePanel *m_sidePanel = nullptr;
  TranscriptPanel *m_transcriptPanel = nullptr;

  TranscriptStore *m_transcriptStore = nullptr;

  AutomationStrip *m_automationStrip = nullptr;
  SessionSettings m_sessionSettings;

  PayloadLogger *m_payloadLogger = nullptr;

  QSpinBox *m_toolCallDepthSpin = nullptr;

  QLineEdit *m_input = nullptr;
  QPushButton *m_sendButton = nullptr;

  OverseerSession *m_currentSession = nullptr;

  QLabel *m_sessionHeader = nullptr;

  OverseerToolRegistry m_tools;

  QJsonArray m_turnMessages;

  InferenceService::RequestToken m_activeToken;

  int m_toolCallDepth = 0;
  int m_toolCallDepthLimit = 16;

  QString m_assistantRawText;

  QList<MemoryProposal> m_proposals;

  QString m_focusedFilePath;
  TextDocument *m_focusedDocument = nullptr;
  TextEdit *m_focusedEditor = nullptr;

  Workstation *m_workstation = nullptr;

  QHash<QString, ScopedSession> m_scopedSessions;
};