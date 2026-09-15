#pragma once

#include "OverseerTool.h"
#include "OverseerToolRegistry.h"
#include "ThemeAware.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QStringList>
#include <QWidget>

class OverseerOverviewEditor;
class OverseerSession;
class TextBrowser;

class InferenceService;

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

class OverseerWidget : public QWidget, public ThemeAware {
  Q_OBJECT

public:
  explicit OverseerWidget(InferenceService *inferenceService,
                          QWidget *parent = nullptr);

  void setThemeTokens(const ThemeTokens &tokens) override;

public slots:
  void addOverviewReference(const QString &path);
  void addOverviewReferences(const QStringList &paths);

private slots:
  void onNewSessionRequested();
  void onSessionSelected();
  void onSendClicked();
  void onSaveMemoryClicked();
  void onRefreshOverviewClicked();
  void onAddOverviewReferenceClicked();
  void onSessionChangedExternally();
  void onToolCallDepthChanged(int value);
  void onOverviewFilesDropped(const QStringList &paths);

  void onLlmToolCalls(const QJsonArray &toolCalls);

private:
  void rebuildSessionList();
  void openSession(OverseerSession *session);
  void closeSession();

  void loadMemoryIntoEditor();
  void loadOverviewIntoEditor();

  void appendTranscriptEntry(const QString &role, const QString &text);
  void appendAssistantChunk(const QString &text);
  void finishAssistantBlock();

  void renderTranscript();

  QString buildSystemPrompt() const;

  void dispatchChatRequest();

  void executeToolCalls(const QJsonArray &toolCalls);

  OverseerTool::Context currentToolContext() const;

  void logToolHumanReadable(const QString &toolName,
                            const QJsonObject &arguments,
                            const OverseerTool::Result &result);

  void logToolDetailed(const QString &toolName,
                       const QJsonObject &arguments,
                       const OverseerTool::Result &result,
                       qint64 durationMs);

  void appendOverviewPaths(const QStringList &paths);

  InferenceService *m_inferenceService = nullptr;

  QListWidget *m_sessionList = nullptr;
  QPushButton *m_newSessionButton = nullptr;

  TextBrowser *m_transcript = nullptr;
  QLineEdit *m_input = nullptr;
  QPushButton *m_sendButton = nullptr;

  QPlainTextEdit *m_memoryEditor = nullptr;
  QPushButton *m_saveMemoryButton = nullptr;

  OverseerOverviewEditor *m_overviewEditor = nullptr;
  QPushButton *m_refreshOverviewButton = nullptr;
  QPushButton *m_addOverviewButton = nullptr;

  QTextEdit *m_toolLog = nullptr;

  QSpinBox *m_toolCallDepthSpin = nullptr;

  QTabWidget *m_sideTabs = nullptr;
  QSplitter *m_mainSplitter = nullptr;

  OverseerSession *m_currentSession = nullptr;

  QLabel *m_sessionHeader = nullptr;

  OverseerToolRegistry m_tools;

  QJsonArray m_turnMessages;

  bool m_expectingLlmResponse = false;

  int m_toolCallDepth = 0;
  int m_toolCallDepthLimit = 16;

  QString m_assistantRawText;
  QString m_lastRenderedText;
};