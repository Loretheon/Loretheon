#pragma once

#include "ConductorTypes.h"
#include "SessionSettings.h"
#include "ThemeAware.h"

#include <QString>
#include <QWidget>

class TextEdit;
class TextDocument;
class AutomationStrip;
class ConductorQueue;
class ConductorRoster;
class DependencyGraph;
class FileWidget;
class MemoryProposalCard;
class OverseerRunner;
class OverseerSessionManager;
class OverseerSessionList;
class OverseerSidePanel;
class TranscriptPanel;
class TranscriptStore;
class Workstation;
class WorkstationBar;

class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QSplitter;
class QStackedWidget;
class QToolButton;
class QVBoxLayout;

class OverseerWidget : public QWidget, public ThemeAware {
  Q_OBJECT

public:
  OverseerWidget(OverseerSessionManager *manager,
                 QWidget *parent = nullptr);

  ~OverseerWidget() override;

  void setThemeTokens(const ThemeTokens &tokens) override;

  void setWorkstation(Workstation *workstation);

  OverseerSessionManager *manager() const { return m_manager; }

  OverseerSessionList *sessionListPanel() const { return m_sessionListPanel; }
  OverseerSidePanel *sidePanel() const { return m_sidePanel; }
  TranscriptPanel *transcriptPanel() const { return m_transcriptPanel; }
  AutomationStrip *automationStrip() const { return m_automationStrip; }
  QLabel *sessionHeader() const { return m_sessionHeader; }
  TranscriptStore *transcriptStore() const;

  ConductorQueue *queue() const;
  ConductorRoster *roster() const;
  DependencyGraph *dependencies();

  OverseerRunner *boundRunner() const { return m_boundRunner; }

  QString activeSessionName() const { return m_activeSessionName; }
  OverseerRunner *m_boundRunner = nullptr;

public slots:

  void retryRequest(const QString &requestId);
  void skipRequest(const QString &requestId);
  void setFocusedFilePath(const QString &absolutePath);
  void setFocusedDocument(TextDocument *document, TextEdit *editor);

  void openSessionByName(const QString &name);

  QString submitRequest(const QString &text);
  QString submitRequestFromLore(const QString &text);

  void cancelRequest(const QString &requestId);
  void removeFailedRequest(const QString &requestId);

  void addOverviewReference(const QString &path);
  void addOverviewReferences(const QStringList &paths);

  // Called by OverseerPage when the composer's depth spin changes.
  // Mirrors what used to happen when OverseerWidget owned the spin.
  void onToolCallDepthChanged(int value);

signals:
  void fileWritten(const QString &absolutePath);
  void fileOpenRequested(const QString &absolutePath);
  void fileCloseRequested(const QString &absolutePath);

  void planGenerationStarted(const QString &absolutePath);
  void planReviewReady(const QString &absolutePath);
  void planApplied(const QString &absolutePath);
  void planFailed(const QString &absolutePath);

  void saveWorkstationFileRequested(const QString &absolutePath);

  // Emitted whenever the bound runner changes, so that views that hold
  // a queue, roster, or dependency graph pointer can re-bind. The page
  // uses this to re-push the runner's queue into the conductor dock.
  void runnerBound(OverseerRunner *runner);

  // A short status line for the page to show in its status area.
  void statusMessage(const QString &text, int timeoutMs);

  // Emitted whenever the enabled state of the composer should change,
  // so OverseerPage can propagate it to the bottom panel without
  // OverseerWidget holding a pointer to the composer.
  void composerEnabledChanged(bool enabled);

private slots:
  void onSessionSelected(const QString &name);
  void onSessionCleared();
  void onNewSessionRequested();

  void onProposalAccepted(const QString &key, const QString &scope);
  void onProposalRejected(const QString &key);

  void onPlanEditAccepted(const QString &planId, int editId);
  void onPlanEditRejected(const QString &planId, int editId);
  void onPlanApplyRequested(const QString &planId);
  void onPlanCancelRequested(const QString &planId);

  void onAutomationSettingsChanged(const SessionSettings &settings);

  void onManagerSessionOpened(const QString &name);
  void onManagerSessionListChanged();
  void onRunnerChanged();
  void onRunnerRequestFinished(const QString &sessionName,
                               const QString &requestId, bool ok,
                               const QString &summary,
                               const QString &filePath);
  void onAgentDepthLimitReached(const QString &agentId, int limit);

  // Rebuild the side panel's "User actions" tab from the bound
  // runner's current pending actions.
  void refreshUserActions();

  // Focus the transcript on a given plan so a side panel click can
  // jump the user to the right card.
  void focusPlanInTranscript(const QString &planId);

private:
  void rebuildSessionList();
  void bindToRunner(OverseerRunner *runner);
  void unbindFromRunner(OverseerRunner *runner);

  OverseerSessionManager *m_manager = nullptr;

  OverseerSessionList *m_sessionListPanel = nullptr;
  OverseerSidePanel *m_sidePanel = nullptr;
  TranscriptPanel *m_transcriptPanel = nullptr;

  AutomationStrip *m_automationStrip = nullptr;

  QLabel *m_sessionHeader = nullptr;

  QSplitter *m_centerSplitter = nullptr;

  Workstation *m_workstation = nullptr;

  QString m_activeSessionName;
};