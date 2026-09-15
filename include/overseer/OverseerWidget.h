#pragma once

#include "ThemeAware.h"

#include <QWidget>

class OverseerSession;

class InferenceService;

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;
class QSplitter;
class QTabWidget;
class QTextEdit;

class OverseerWidget : public QWidget, public ThemeAware {
  Q_OBJECT

public:
  explicit OverseerWidget(InferenceService *inferenceService,
                          QWidget *parent = nullptr);

  void setThemeTokens(const ThemeTokens &tokens) override;

private slots:
  void onNewSessionRequested();
  void onSessionSelected();
  void onSendClicked();
  void onSaveMemoryClicked();
  void onRefreshOverviewClicked();
  void onAddOverviewReferenceClicked();
  void onSessionChangedExternally();

private:
  void rebuildSessionList();
  void openSession(OverseerSession *session);
  void closeSession();

  void loadMemoryIntoEditor();
  void loadOverviewIntoEditor();

  void appendTranscriptEntry(const QString &role, const QString &text);

  QString buildSystemPrompt() const;

  InferenceService *m_inferenceService = nullptr;

  QListWidget *m_sessionList = nullptr;
  QPushButton *m_newSessionButton = nullptr;

  QTextEdit *m_transcript = nullptr;
  QLineEdit *m_input = nullptr;
  QPushButton *m_sendButton = nullptr;

  QPlainTextEdit *m_memoryEditor = nullptr;
  QPushButton *m_saveMemoryButton = nullptr;

  QPlainTextEdit *m_overviewEditor = nullptr;
  QPushButton *m_refreshOverviewButton = nullptr;
  QPushButton *m_addOverviewButton = nullptr;

  QTabWidget *m_sideTabs = nullptr;
  QSplitter *m_mainSplitter = nullptr;

  OverseerSession *m_currentSession = nullptr;

  QLabel *m_sessionHeader = nullptr;

  bool m_assistantMessageOpen = false;
};