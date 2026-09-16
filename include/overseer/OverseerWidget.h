#pragma once

#include "OverseerTool.h"
#include "OverseerToolRegistry.h"
#include "ThemeAware.h"
#include "TranscriptEvent.h"

#include "inference/InferenceService.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>
#include <QWidget>

class MemoryProposalCard;
class OverseerSession;
class OverseerSessionList;
class OverseerSidePanel;
class ToastStack;
class TranscriptPanel;
class TranscriptStore;

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

  void setThemeTokens(const ThemeTokens &tokens) override;

  OverseerSessionList *sessionListPanel() const { return m_sessionListPanel; }
  OverseerSidePanel *sidePanel() const { return m_sidePanel; }
  TranscriptPanel *transcriptPanel() const { return m_transcriptPanel; }
  TranscriptStore *transcriptStore() const { return m_transcriptStore; }

  OverseerSession *currentSession() const { return m_currentSession; }

  signals:
  void fileWritten(const QString &absolutePath);


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

  void onProposalAccepted(const QString &key);
  void onProposalRejected(const QString &key);

private:
  struct MemoryProposal {
    QString key;
    QString fact;
    QString rationale;
    QString status;
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

  QString recordProposal(const QString &fact, const QString &rationale);
  void setProposalStatus(const QString &key, const QString &status);
  QString proposalsSidecarPath() const;
  void loadProposals();
  void saveProposals();
  void appendFactToMemory(const QString &fact);

  InferenceService *m_inferenceService = nullptr;

  OverseerSessionList *m_sessionListPanel = nullptr;
  OverseerSidePanel *m_sidePanel = nullptr;
  TranscriptPanel *m_transcriptPanel = nullptr;

  TranscriptStore *m_transcriptStore = nullptr;

  QSpinBox *m_toolCallDepthSpin = nullptr;

  QLineEdit *m_input = nullptr;
  QPushButton *m_sendButton = nullptr;

  ToastStack *m_toastStack = nullptr;

  OverseerSession *m_currentSession = nullptr;

  QLabel *m_sessionHeader = nullptr;

  OverseerToolRegistry m_tools;

  QJsonArray m_turnMessages;

  InferenceService::RequestToken m_activeToken;

  int m_toolCallDepth = 0;
  int m_toolCallDepthLimit = 16;

  QString m_assistantRawText;

  QList<MemoryProposal> m_proposals;
};