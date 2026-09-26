#pragma once

#include "ConductorTypes.h"
#include "SessionSettings.h"
#include "ThemeAware.h"

#include <QString>
#include <QWidget>

class TextEdit;
class TextDocument;
class AutomationStrip;
class ConductorDock;
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
  TranscriptStore *transcriptStore() const;

  ConductorQueue *queue() const;
  ConductorRoster *roster() const;
  DependencyGraph *dependencies();

  QString activeSessionName() const { return m_activeSessionName; }
  OverseerRunner *m_boundRunner = nullptr;

public slots:
  void setFocusedFilePath(const QString &absolutePath);
  void setFocusedDocument(TextDocument *document, TextEdit *editor);

  void openSessionByName(const QString &name);

  QString submitRequest(const QString &text);
  QString submitRequestFromLore(const QString &text);

  void cancelRequest(const QString &requestId);
  void removeFailedRequest(const QString &requestId);

  void addOverviewReference(const QString &path);
  void addOverviewReferences(const QStringList &paths);

signals:
  void fileWritten(const QString &absolutePath);
  void fileOpenRequested(const QString &absolutePath);
  void fileCloseRequested(const QString &absolutePath);

  void planGenerationStarted(const QString &absolutePath);
  void planReviewReady(const QString &absolutePath);
  void planApplied(const QString &absolutePath);
  void planFailed(const QString &absolutePath);

  void saveWorkstationFileRequested(const QString &absolutePath);

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

  void onManagerSessionOpened(const QString &name);
  void onManagerSessionListChanged();
  void onRunnerChanged();
  void onRunnerRequestFinished(const QString &sessionName,
                               const QString &requestId, bool ok,
                               const QString &summary,
                               const QString &filePath);

private:
  void rebuildSessionList();
  void bindToRunner(OverseerRunner *runner);
  void unbindFromRunner(OverseerRunner *runner);

  OverseerSessionManager *m_manager = nullptr;

  OverseerSessionList *m_sessionListPanel = nullptr;
  OverseerSidePanel *m_sidePanel = nullptr;
  TranscriptPanel *m_transcriptPanel = nullptr;

  AutomationStrip *m_automationStrip = nullptr;
  ConductorDock *m_conductorDock = nullptr;
  QToolButton *m_dockTrigger = nullptr;

  QLabel *m_sessionHeader = nullptr;
  QLineEdit *m_input = nullptr;
  QPushButton *m_sendButton = nullptr;
  QSpinBox *m_toolCallDepthSpin = nullptr;

  QSplitter *m_centerSplitter = nullptr;

  Workstation *m_workstation = nullptr;

  QString m_activeSessionName;

};