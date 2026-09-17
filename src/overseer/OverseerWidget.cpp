#include "OverseerWidget.h"

#include "AutomationStrip.h"
#include "ChatWidgetSerialization.h"
#include "EditNoteReviewDialog.h"
#include "EditPlanner.h"
#include "EditSession.h"
#include "MemoryPanel.h"
#include "MemoryProposalCard.h"
#include "OverseerSession.h"
#include "OverseerSessionList.h"
#include "OverseerSidePanel.h"
#include "OverseerStorage.h"
#include "OverseerTools.h"
#include "OverviewPanel.h"
#include "PathUtils.h"
#include "PayloadLogger.h"
#include "PendingEdit.h"
#include "Settings.h"
#include "ToastStack.h"
#include "TranscriptPanel.h"
#include "TranscriptStore.h"

#include "TextEdit.h"

#include "inference/InferenceService.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QSplitter>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextStream>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>

namespace {

QString formatMessagesForLog(const QJsonArray &messages) {
  QString out;

  for (int i = 0; i < messages.size(); ++i) {
    const QJsonObject message = messages.at(i).toObject();
    const QString role = message.value(QStringLiteral("role")).toString();
    const QString content = message.value(QStringLiteral("content")).toString();

    out += QStringLiteral("[message %1 | role=%2]\n%3\n\n")
               .arg(i)
               .arg(role, content);
  }

  return out;
}

QString formatToolsForLog(const QJsonArray &schemas) {
  return QString::fromUtf8(
      QJsonDocument(schemas).toJson(QJsonDocument::Indented));
}

} // namespace

OverseerWidget::OverseerWidget(InferenceService *inferenceService,
                               QWidget *parent)
    : QWidget(parent), m_inferenceService(inferenceService) {
  OverseerStorage::ensureRoot();

  OverseerTools::installAll(m_tools);

  m_payloadLogger = new PayloadLogger(this);

  m_toolCallDepthLimit = Settings::getOverseerToolCallDepthLimit();

  m_sessionListPanel = new OverseerSessionList(this);
  m_transcriptStore = new TranscriptStore(this);
  m_transcriptPanel = new TranscriptPanel(m_transcriptStore, this);
  m_sidePanel = new OverseerSidePanel(this);

  auto *centerPanel = new QWidget(this);
  auto *centerLayout = new QVBoxLayout(centerPanel);
  centerLayout->setContentsMargins(6, 6, 6, 6);
  centerLayout->setSpacing(6);

  m_sessionHeader = new QLabel(tr("No session"), centerPanel);
  QFont headerFont = m_sessionHeader->font();
  headerFont.setBold(true);
  m_sessionHeader->setFont(headerFont);
  centerLayout->addWidget(m_sessionHeader);

  m_automationStrip = new AutomationStrip(centerPanel);
  m_automationStrip->setEnabledState(false);
  centerLayout->addWidget(m_automationStrip);

  auto *inputRow = new QHBoxLayout;
  m_input = new QLineEdit(centerPanel);
  m_input->setPlaceholderText(tr("Ask Overseer…"));
  m_sendButton = new QPushButton(tr("Send"), centerPanel);

  auto *depthLabel = new QLabel(tr("Tool depth:"), centerPanel);
  m_toolCallDepthSpin = new QSpinBox(centerPanel);
  m_toolCallDepthSpin->setRange(1, 64);
  m_toolCallDepthSpin->setValue(m_toolCallDepthLimit);

  inputRow->addWidget(m_input, 1);
  inputRow->addWidget(m_sendButton);
  inputRow->addSpacing(12);
  inputRow->addWidget(depthLabel);
  inputRow->addWidget(m_toolCallDepthSpin);
  centerLayout->addLayout(inputRow);

  auto *rootLayout = new QVBoxLayout(this);
  rootLayout->setContentsMargins(0, 0, 0, 0);
  rootLayout->addWidget(centerPanel);

  m_toastStack = new ToastStack(this);

  connect(m_sessionListPanel, &OverseerSessionList::newSessionRequested, this,
          &OverseerWidget::onNewSessionRequested);

  connect(m_sessionListPanel, &OverseerSessionList::sessionSelected, this,
          &OverseerWidget::onSessionSelected);

  connect(m_sessionListPanel, &OverseerSessionList::sessionCleared, this,
          &OverseerWidget::onSessionCleared);

  connect(m_automationStrip, &AutomationStrip::settingsChanged, this,
          &OverseerWidget::onAutomationSettingsChanged);

  connect(m_sendButton, &QPushButton::clicked, this,
          &OverseerWidget::onSendClicked);

  connect(m_input, &QLineEdit::returnPressed, this,
          &OverseerWidget::onSendClicked);

  connect(m_toolCallDepthSpin,
          QOverload<int>::of(&QSpinBox::valueChanged), this,
          &OverseerWidget::onToolCallDepthChanged);

  connect(m_transcriptPanel, &TranscriptPanel::memoryProposalAccepted, this,
          &OverseerWidget::onProposalAccepted);

  connect(m_transcriptPanel, &TranscriptPanel::memoryProposalRejected, this,
          &OverseerWidget::onProposalRejected);

  connect(m_transcriptPanel, &TranscriptPanel::planEditAccepted, this,
          &OverseerWidget::onPlanEditAccepted);

  connect(m_transcriptPanel, &TranscriptPanel::planEditRejected, this,
          &OverseerWidget::onPlanEditRejected);

  connect(m_transcriptPanel, &TranscriptPanel::planApplyRequested, this,
          &OverseerWidget::onPlanApplyRequested);

  connect(m_transcriptPanel, &TranscriptPanel::planCancelRequested, this,
          &OverseerWidget::onPlanCancelRequested);

  if (m_sidePanel && m_sidePanel->overviewPanel()) {
    connect(m_sidePanel->overviewPanel(), &OverviewPanel::openRequested, this,
            [this](const QString &relativePath) {
              if (m_currentSession) {
                const QString abs = PathUtils::toAbsolute(
                    relativePath, m_currentSession->outputPath());
                Q_UNUSED(abs);
              }
            });

    connect(m_sidePanel->overviewPanel(),
            &OverviewPanel::openInNormalEditorRequested, this,
            [this](const QString &relativePath) {
              Q_UNUSED(relativePath);
            });

    connect(m_sidePanel->overviewPanel(), &OverviewPanel::stageRequested, this,
            [this](const QString &relativePath) { Q_UNUSED(relativePath); });
  }

  if (m_inferenceService) {
    connect(m_inferenceService, &InferenceService::llmDelta, this,
            [this](const InferenceService::RequestToken &token,
                   const QString &text) {
              if (m_generatingEdit && token == m_generationToken) {
                onGenerationDelta(token, text);
                return;
              }

              if (token != m_activeToken || !m_currentSession)
                return;

              m_assistantRawText += text;
            });

    connect(m_inferenceService, &InferenceService::llmFinished, this,
            [this](const InferenceService::RequestToken &token) {
              if (m_generatingEdit && token == m_generationToken) {
                onGenerationFinished(token);
                return;
              }

              if (token != m_activeToken)
                return;

              m_activeToken = InferenceService::RequestToken();

              if (!m_currentSession)
                return;

              // ---- CHANGE 1: log the response, or surface an empty one ----
              if (!m_assistantRawText.isEmpty()) {
                TranscriptEvent event;
                event.type = TranscriptEvent::Type::AssistantMessage;
                event.role = QStringLiteral("assistant");
                event.body = m_assistantRawText;
                appendEvent(event);

                if (m_payloadLogger) {
                  m_payloadLogger->log(
                      QStringLiteral("OVERSEER_CHAT_RESPONSE"),
                      QStringLiteral("Session: %1\n\nAssistant text:\n%2")
                          .arg(m_currentSession->name(),
                               m_assistantRawText));
                }
              } else {
                if (m_payloadLogger) {
                  m_payloadLogger->log(
                      QStringLiteral("OVERSEER_CHAT_EMPTY"),
                      QStringLiteral(
                          "Session: %1\n\n"
                          "The assistant returned no text and no tool calls.")
                          .arg(m_currentSession->name()));
                }

                TranscriptEvent notice;
                notice.type = TranscriptEvent::Type::Notice;
                notice.role = QStringLiteral("notice");
                notice.body =
                    tr("The assistant returned an empty response.");
                appendEvent(notice);
              }
              // ---- end CHANGE 1 ----

              m_turnMessages = QJsonArray();
              m_toolCallDepth = 0;
            });

    connect(m_inferenceService, &InferenceService::llmToolCalls, this,
            [this](const InferenceService::RequestToken &token,
                   const QJsonArray &toolCalls) {
              if (token != m_activeToken)
                return;

              m_activeToken = InferenceService::RequestToken();

              if (!m_currentSession)
                return;

              if (m_payloadLogger) {
                m_payloadLogger->log(
                    QStringLiteral("OVERSEER_CHAT_TOOL_CALLS"),
                    QStringLiteral("Session: %1\n\nTool calls:\n%2")
                        .arg(m_currentSession->name(),
                             QString::fromUtf8(
                                 QJsonDocument(toolCalls)
                                     .toJson(QJsonDocument::Indented))));
              }

              handleToolCalls(toolCalls);
            });

    connect(m_inferenceService, &InferenceService::llmError, this,
            [this](const InferenceService::RequestToken &token,
                   const QString &error) {
              if (m_generatingEdit && token == m_generationToken) {
                onGenerationError(token, error);
                return;
              }

              if (token != m_activeToken)
                return;

              m_activeToken = InferenceService::RequestToken();

              if (!m_currentSession)
                return;

              if (m_payloadLogger) {
                m_payloadLogger->log(
                    QStringLiteral("OVERSEER_CHAT_ERROR"),
                    QStringLiteral("Session: %1\n\nError: %2")
                        .arg(m_currentSession->name(), error));
              }

              TranscriptEvent event;
              event.type = TranscriptEvent::Type::Error;
              event.role = QStringLiteral("error");
              event.body = error;
              appendEvent(event);

              m_turnMessages = QJsonArray();
              m_toolCallDepth = 0;
            });
  }

  rebuildSessionList();

  m_input->setEnabled(false);
  m_sendButton->setEnabled(false);
}

OverseerWidget::~OverseerWidget() = default;

void OverseerWidget::setThemeTokens(const ThemeTokens &tokens) {
  Q_UNUSED(tokens);
}

void OverseerWidget::setFocusedFilePath(const QString &absolutePath) {
  m_focusedFilePath = absolutePath;
}

void OverseerWidget::setFocusedDocument(TextDocument *document,
                                        TextEdit *editor) {
  m_focusedDocument = document;
  m_focusedEditor = editor;
}

void OverseerWidget::appendEvent(const TranscriptEvent &event) {
  if (m_transcriptStore)
    m_transcriptStore->append(event);
}

void OverseerWidget::rebuildSessionList() {
  m_sessionListPanel->rebuild(
      OverseerSession::list(OverseerStorage::rootPath()));
}

void OverseerWidget::onNewSessionRequested() {
  bool ok = false;

  const QString name = QInputDialog::getText(
      this, tr("New Overseer Session"), tr("Session name:"), QLineEdit::Normal,
      QString(), &ok);

  if (!ok)
    return;

  const QString trimmed = name.trimmed();

  if (trimmed.isEmpty())
    return;

  OverseerSession *session =
      OverseerSession::create(OverseerStorage::rootPath(), trimmed, this);

  if (!session) {
    QMessageBox::warning(
        this, tr("New Overseer Session"),
        tr("A session named '%1' already exists, or the name is invalid.")
            .arg(trimmed));
    return;
  }

  rebuildSessionList();
  m_sessionListPanel->selectByName(trimmed);
}

void OverseerWidget::onSessionSelected(const QString &name) {
  if (name.isEmpty()) {
    closeSession();
    return;
  }

  OverseerSession *session =
      OverseerSession::open(OverseerStorage::rootPath(), name, this);

  if (!session) {
    closeSession();
    return;
  }

  openSession(session);
}

void OverseerWidget::onSessionCleared() { closeSession(); }

void OverseerWidget::openSessionByName(const QString &name) {
  onSessionSelected(name);
}

void OverseerWidget::openSession(OverseerSession *session) {
  closeSession();

  m_currentSession = session;

  connect(m_currentSession, &OverseerSession::changed, this, [this]() {
    if (m_sidePanel && m_sidePanel->overviewPanel() && m_currentSession)
      m_sidePanel->overviewPanel()->loadFromFile(
          m_currentSession->overviewPath());
  });

  m_sessionHeader->setText(tr("Session: %1").arg(session->name()));

  SessionSettings::ensureFile(session->settingsPath());

  m_sessionSettings = SessionSettings::load(session->settingsPath());
  m_automationStrip->setSettings(m_sessionSettings);
  m_automationStrip->setEnabledState(true);

  if (m_transcriptStore) {
    m_transcriptStore->setSession(session);
  }

  if (m_sidePanel) {
    if (auto *op = m_sidePanel->overviewPanel()) {
      op->setNotesRoot(Settings::getRootDirectory());
      op->loadFromFile(session->overviewPath());
    }
  }

  reloadMemoryPanels();

  loadProposals();

  m_input->setEnabled(true);
  m_sendButton->setEnabled(true);
}

void OverseerWidget::closeSession() {
  if (!m_currentSession)
    return;

  if (m_inferenceService && !m_activeToken.isNull()) {
    m_inferenceService->abortChatRequest(m_activeToken);
    m_activeToken = InferenceService::RequestToken();
  }

  if (m_toastStack)
    m_toastStack->dismissAll();

  tearDownScopedEditSession();

  disconnect(m_currentSession, nullptr, this, nullptr);

  m_currentSession->deleteLater();
  m_currentSession = nullptr;

  if (m_transcriptStore)
    m_transcriptStore->clear();

  m_sessionHeader->setText(tr("No session"));

  m_sessionSettings = SessionSettings();
  m_automationStrip->setSettings(m_sessionSettings);
  m_automationStrip->setEnabledState(false);

  m_proposals.clear();
  m_turnMessages = QJsonArray();
  m_toolCallDepth = 0;
  m_assistantRawText.clear();

  m_input->setEnabled(false);
  m_sendButton->setEnabled(false);
}

QString OverseerWidget::buildSystemPrompt() const {
  const QString globalMemory = OverseerStorage::readMemory();
  const QString sessionMemory =
      m_currentSession ? m_currentSession->memory() : QString();
  const QString overview =
      m_currentSession ? m_currentSession->overview() : QString();

  QString workspaceTree;

  if (m_currentSession) {
    const QString output = m_currentSession->outputPath();

    QStringList entries;

    QDirIterator it(output, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                    QDirIterator::Subdirectories);

    while (it.hasNext() && entries.size() < 200) {
      const QString rel = QDir(output).relativeFilePath(it.next());

      if (rel == QStringLiteral("workstation.json"))
        continue;

      entries.append(rel);
    }

    entries.sort();

    if (entries.isEmpty()) {
      workspaceTree =
          QStringLiteral("(empty — you may create files directly here)");
    } else {
      workspaceTree = entries.join(QChar('\n'));
    }
  }

  QString focusSection;

  if (!m_focusedFilePath.isEmpty()) {
    const QString rel =
        m_currentSession
            ? QDir(m_currentSession->outputPath())
                  .relativeFilePath(m_focusedFilePath)
            : m_focusedFilePath;

    focusSection += QStringLiteral("## Focused file\n\n");
    focusSection += QStringLiteral("The user is currently looking at: %1\n")
                        .arg(rel);
    focusSection += QStringLiteral(
        "To edit this file, call edit_workstation_file. Do not pass a "
        "path; the tool always targets the focused window. The user "
        "reviews the generated edits before they are applied.\n\n");
  }

  QString prompt;

  prompt += QStringLiteral(
      "You are Overseer, a persistent assistant working inside a "
      "per-session workspace on the user's machine.\n"
      "\n"
      "## Workspace\n"
      "\n"
      "You have one folder you can write to: the session's output folder. "
      "Every path you pass to write_file, create_directory, read_file, "
      "list_directory, open_file, and close_file is interpreted **relative "
      "to that folder**. Paths must not start with a slash, must not start "
      "with a drive letter, and must not contain `..`. The output folder "
      "*is* the root of your workspace — do not prefix paths with its name "
      "or with a folder that does not yet exist.\n"
      "\n"
      "Current contents of the output folder:\n"
      "%1\n"
      "\n"
      "## Tools\n"
      "\n"
      "  - list_directory(path) — list the entries under `path`. Pass an "
      "empty string or `.` to list the root of the output folder.\n"
      "  - read_file(path) — read a file inside the output folder. Use "
      "this to read any file you or the user created in this session. "
      "Paths are relative to the output folder; do not prefix them with "
      "`output/` or `notes/` unless the file really lives in a "
      "subdirectory by that name.\n"
      "  - write_file(path, content) — create or overwrite a file inside "
      "the output folder. Parent directories are created automatically.\n"
      "  - create_directory(path) — create a directory inside the output "
      "folder.\n"
      "  - open_file(path) — open a file from the output folder in the "
      "Workstation so the user can see it.\n"
      "  - close_file(path) — close a file that is open in the Workstation.\n"
      "  - edit_workstation_file(instruction) — start a scoped edit session "
      "on the file the user is currently looking at. The user reviews the "
      "generated edits before they are applied. Call this whenever the "
      "user asks you to change, add, remove, rewrite, or fill in content "
      "in a file. Do not reply with prose describing what you would do; "
      "call the tool.\n"
      "  - read_notes_file(path) — read-only access to files under the "
      "user's notes root. This is a different root from the session "
      "output folder. Only use this tool when the user explicitly refers "
      "to a file in their notes library, not for session files.\n"
      "  - edit_note(path, instruction) — open a copy of a notes-root file "
      "for the user to review and apply edits to. The original note is not "
      "modified until the user promotes it.\n"
      "  - propose_global_memory_fact(fact, rationale) — propose a durable "
      "fact that should persist across every session. Use this for standing "
      "user preferences and rules.\n"
      "  - propose_session_memory_fact(fact, rationale) — propose a fact "
      "scoped to this session only. Use this for details that matter here "
      "but should not become standing rules.\n"
      "\n"
      "## Rules\n"
      "\n"
      "  - Use relative paths. `outline.md` is correct; `story/outline.md` "
      "creates a `story` subdirectory. `output/outline.md` is wrong and "
      "will be rejected.\n"
      "  - When the user asks you to set up a workspace, create files "
      "directly in the output folder unless the user specifically asks "
      "for subdirectories.\n"
      "  - Before creating a file, check the workspace listing above. If "
      "an equivalent file already exists, do not create a duplicate — "
      "read it and edit it.\n"
      "  - When a tool returns an error, read the error message carefully "
      "and adjust. Do not retry the same call unchanged.\n"
      "  - When the user asks for a change to a file, always call "
      "edit_workstation_file. Do not reply with prose, do not ask for "
      "clarification, and do not describe what you would do.\n"
      "  - When your task is complete, reply with a short summary. Do not "
      "call more tools after that.\n"
      "\n")
      .arg(workspaceTree);

  prompt += focusSection;

  prompt += QStringLiteral("## Global Memory\n\n%1\n\n").arg(globalMemory);
  prompt += QStringLiteral("## Session Memory\n\n%1\n\n").arg(sessionMemory);
  prompt += QStringLiteral("## Session Overview\n\n%1\n\n").arg(overview);

  return prompt;
}

OverseerTool::Context OverseerWidget::currentToolContext() const {
  OverseerTool::Context context;

  if (m_currentSession) {
    context.sessionFolder = m_currentSession->folderPath();
    context.outputFolder = m_currentSession->outputPath();
  }

  context.notesRoot = Settings::getRootDirectory();

  context.focusedFilePath = m_focusedFilePath;
  context.focusedDocument = m_focusedDocument;
  context.focusedEditor = m_focusedEditor;

  auto *self = const_cast<OverseerWidget *>(this);

  context.requestEditNoteReview =
      [self](const QString &copyPath, const QString &originalPath,
             const QString &instruction) {
        if (!self->m_inferenceService)
          return;

        auto *dialog = new EditNoteReviewDialog(
            self->m_inferenceService, copyPath, originalPath, instruction,
            self);

        dialog->setAttribute(Qt::WA_DeleteOnClose, true);
        dialog->show();
        dialog->raise();
        dialog->activateWindow();
      };

  context.openFile = [self](const QString &absolutePath) {
    emit self->fileOpenRequested(absolutePath);
  };

  context.closeFile = [self](const QString &absolutePath) {
    emit self->fileCloseRequested(absolutePath);
  };

  context.requestScopedEdit = [self](TextEdit *editor, TextDocument *document,
                                     const QString &instruction) {
    self->startScopedEdit(editor, document, instruction);
  };

  return context;
}

void OverseerWidget::dispatchChatRequest() {
  if (!m_inferenceService || !m_currentSession)
    return;

  if (m_payloadLogger) {
    m_payloadLogger->log(
        QStringLiteral("OVERSEER_CHAT_REQUEST"),
        QStringLiteral("Session: %1\n\nMessages:\n%2\nTools:\n%3")
            .arg(m_currentSession->name(),
                 formatMessagesForLog(m_turnMessages),
                 formatToolsForLog(m_tools.schemas())));
  }

  m_activeToken = m_inferenceService->sendChatRequest(
      m_turnMessages, QString(), 0.7, 120000, QString(), QJsonObject(),
      m_tools.schemas());
}

void OverseerWidget::handleToolCalls(const QJsonArray &toolCalls) {
  bool onlyNonBlocking = !toolCalls.isEmpty();
  bool hasScopedEdit = false;

  for (const QJsonValue &value : toolCalls) {
    if (!value.isObject()) {
      onlyNonBlocking = false;
      break;
    }

    const QJsonObject call = value.toObject();
    const QJsonObject function =
        call.value(QStringLiteral("function")).toObject();
    const QString name = function.value(QStringLiteral("name")).toString();

    if (name == QStringLiteral("edit_workstation_file"))
      hasScopedEdit = true;

    if (name != QStringLiteral("propose_global_memory_fact") &&
        name != QStringLiteral("propose_session_memory_fact") &&
        name != QStringLiteral("edit_note") &&
        name != QStringLiteral("edit_workstation_file")) {
      onlyNonBlocking = false;
    }
  }

  if (!onlyNonBlocking) {
    if (m_toolCallDepth >= m_toolCallDepthLimit) {
      TranscriptEvent event;
      event.type = TranscriptEvent::Type::Error;
      event.role = QStringLiteral("error");
      event.body =
          tr("Tool call depth limit reached (%1).").arg(m_toolCallDepthLimit);
      appendEvent(event);

      m_turnMessages = QJsonArray();
      m_toolCallDepth = 0;
      return;
    }

    ++m_toolCallDepth;
  }

  executeToolCalls(toolCalls);

  if (hasScopedEdit)
    return;

  dispatchChatRequest();
}

void OverseerWidget::executeToolCalls(const QJsonArray &toolCalls) {
  const OverseerTool::Context context = currentToolContext();

  for (const QJsonValue &value : toolCalls) {
    if (!value.isObject())
      continue;

    const QJsonObject call = value.toObject();
    const QString callId = call.value(QStringLiteral("id")).toString();
    const QJsonObject function =
        call.value(QStringLiteral("function")).toObject();
    const QString name = function.value(QStringLiteral("name")).toString();
    const QString rawArguments =
        function.value(QStringLiteral("arguments")).toString();

    QJsonObject arguments;

    if (!rawArguments.isEmpty()) {
      QJsonParseError parseError;
      const QJsonDocument document =
          QJsonDocument::fromJson(rawArguments.toUtf8(), &parseError);

      if (parseError.error == QJsonParseError::NoError &&
          document.isObject()) {
        arguments = document.object();
      }
    }

    const qint64 started = QDateTime::currentMSecsSinceEpoch();
    const OverseerTool::Result result =
        m_tools.execute(name, arguments, context);
    const qint64 durationMs = QDateTime::currentMSecsSinceEpoch() - started;

    TranscriptEvent callEvent;
    callEvent.type = TranscriptEvent::Type::ToolCall;
    callEvent.role = QStringLiteral("tool");
    callEvent.toolName = name;
    callEvent.toolCategory = m_tools.categoryFor(name);
    callEvent.toolArguments = arguments;
    callEvent.toolOk = result.ok;
    callEvent.toolDurationMs = durationMs;
    callEvent.body = result.ok ? result.output : result.error;
    appendEvent(callEvent);

    if (result.ok && name == QStringLiteral("write_file") && m_currentSession) {
      const QString rel =
          arguments.value(QStringLiteral("path")).toString();

      const QString abs =
          QDir(m_currentSession->outputPath()).absoluteFilePath(rel);

      emit fileWritten(abs);
    }

    const bool isGlobalProposal =
        name == QStringLiteral("propose_global_memory_fact");
    const bool isSessionProposal =
        name == QStringLiteral("propose_session_memory_fact");

    if (result.ok && (isGlobalProposal || isSessionProposal)) {
      const QString fact =
          arguments.value(QStringLiteral("fact")).toString().trimmed();
      const QString rationale =
          arguments.value(QStringLiteral("rationale")).toString().trimmed();

      const QString scope = isGlobalProposal ? QStringLiteral("global")
                                             : QStringLiteral("session");

      if (!fact.isEmpty()) {
        const QString key = recordProposal(fact, rationale, scope);

        TranscriptEvent proposalEvent;
        proposalEvent.type = TranscriptEvent::Type::MemoryProposal;
        proposalEvent.role = QStringLiteral("proposal");
        proposalEvent.proposalKey = key;
        proposalEvent.proposalFact = fact;
        proposalEvent.proposalRationale = rationale;
        proposalEvent.proposalScope = scope;
        proposalEvent.proposalStatus = QStringLiteral("pending");
        proposalEvent.proposalContext = m_assistantRawText;
        appendEvent(proposalEvent);

        if (m_toastStack) {
          const QString scopeLabel =
              scope == QStringLiteral("global") ? tr("global") : tr("session");
          m_toastStack->showProposalToast(fact, rationale, scopeLabel);
        }

        if (m_sessionSettings.effectiveAutoMemory()) {
          QTimer::singleShot(0, this, [this, key, scope]() {
            if (!m_sessionSettings.effectiveAutoMemory())
              return;

            setProposalStatus(key, QStringLiteral("accepted"), scope);
          });
        }
      }
    }

    QJsonObject assistantFunction;
    assistantFunction.insert(QStringLiteral("name"), name);
    assistantFunction.insert(QStringLiteral("arguments"), rawArguments);

    QJsonObject assistantCall;
    assistantCall.insert(QStringLiteral("id"), callId);
    assistantCall.insert(QStringLiteral("type"), QStringLiteral("function"));
    assistantCall.insert(QStringLiteral("function"), assistantFunction);

    QJsonObject assistantMessage;
    assistantMessage.insert(QStringLiteral("role"), QStringLiteral("assistant"));
    assistantMessage.insert(QStringLiteral("tool_calls"),
                            QJsonArray{assistantCall});

    m_turnMessages.append(assistantMessage);

    QJsonObject toolMessage;
    toolMessage.insert(QStringLiteral("role"), QStringLiteral("tool"));
    toolMessage.insert(QStringLiteral("tool_call_id"), callId);
    toolMessage.insert(QStringLiteral("content"),
                       result.ok ? result.output : result.error);

    m_turnMessages.append(toolMessage);
  }
}

void OverseerWidget::onSendClicked() {
  if (!m_currentSession || !m_inferenceService)
    return;

  const QString prompt = m_input->text().trimmed();
  if (prompt.isEmpty())
    return;

  m_input->clear();

  TranscriptEvent userEvent;
  userEvent.type = TranscriptEvent::Type::UserMessage;
  userEvent.role = QStringLiteral("user");
  userEvent.body = prompt;
  appendEvent(userEvent);

  m_assistantRawText.clear();
  m_toolCallDepth = 0;
  m_turnMessages = QJsonArray();

  m_turnMessages.append(QJsonObject{
      {QStringLiteral("role"), QStringLiteral("system")},
      {QStringLiteral("content"), buildSystemPrompt()}});

  m_turnMessages.append(QJsonObject{
      {QStringLiteral("role"), QStringLiteral("user")},
      {QStringLiteral("content"), prompt}});

  dispatchChatRequest();
}

void OverseerWidget::onToolCallDepthChanged(int value) {
  m_toolCallDepthLimit = qBound(1, value, 64);
  Settings::setOverseerToolCallDepthLimit(m_toolCallDepthLimit);
}

void OverseerWidget::onAutomationSettingsChanged(
    const SessionSettings &settings) {
  m_sessionSettings = settings;
  m_sessionSettings.normalize();

  if (m_currentSession) {
    if (!m_sessionSettings.save(m_currentSession->settingsPath())) {
      qWarning() << "[OverseerWidget] Failed to save session settings to"
                 << m_currentSession->settingsPath();
    }
  }
}

void OverseerWidget::onProposalAccepted(const QString &key,
                                        const QString &scope) {
  setProposalStatus(key, QStringLiteral("accepted"), scope);
}

void OverseerWidget::onProposalRejected(const QString &key) {
  setProposalStatus(key, QStringLiteral("rejected"), QString());
}

void OverseerWidget::startScopedEdit(TextEdit *editor, TextDocument *document,
                                     const QString &instruction) {
  Q_UNUSED(document);

  if (!editor || instruction.isEmpty() || !m_inferenceService)
    return;

  tearDownScopedEditSession();

  m_scopedPlanner = new EditPlanner(m_inferenceService, this);

  connect(m_scopedPlanner, &EditPlanner::planValidated, this,
          &OverseerWidget::onPlannerValidated);

  connect(m_scopedPlanner, &EditPlanner::failed, this,
          &OverseerWidget::onPlannerFailed);

  m_scopedPlanId = QUuid::createUuid().toString(QUuid::WithoutBraces);
  m_scopedPlanFilePath = m_focusedFilePath;
  m_scopedInstruction = instruction;

  m_scopedPlanner->start(editor, instruction);
}

void OverseerWidget::onPlannerValidated(
    const QVector<EditCommand> &commands) {
  m_pendingPlanAwaitingAutoApply = false;

  if (commands.isEmpty()) {
    TranscriptEvent event;
    event.type = TranscriptEvent::Type::Error;
    event.role = QStringLiteral("error");
    event.body = tr("Planner returned no edits.");
    appendEvent(event);

    emit planFailed(m_scopedPlanFilePath);

    tearDownScopedEditSession();
    return;
  }

  if (!m_focusedEditor) {
    TranscriptEvent event;
    event.type = TranscriptEvent::Type::Error;
    event.role = QStringLiteral("error");
    event.body = tr("No focused editor to apply edits to.");
    appendEvent(event);

    emit planFailed(m_scopedPlanFilePath);

    tearDownScopedEditSession();
    return;
  }

  m_scopedCommands = commands;

  m_scopedSession = new EditSession(m_focusedEditor, this);

  connect(m_scopedSession, &EditSession::pendingEditStarted, m_focusedEditor,
          &TextEdit::showPendingEdit);

  connect(m_scopedSession, &EditSession::pendingEditUpdated, m_focusedEditor,
          &TextEdit::updatePendingEdit);

  connect(m_scopedSession, &EditSession::pendingEditFinished, m_focusedEditor,
          [this](const PendingEdit &edit) {
            if (m_focusedEditor)
              m_focusedEditor->updatePendingEdit(edit);
          });

  connect(m_scopedSession, &EditSession::pendingEditsChanged, m_focusedEditor,
          &TextEdit::refreshPendingEdits);

  connect(m_scopedSession, &EditSession::failed, this,
          [this](const QString &reason) {
            TranscriptEvent event;
            event.type = TranscriptEvent::Type::Error;
            event.role = QStringLiteral("error");
            event.body = tr("Edit session failed: %1").arg(reason);
            appendEvent(event);

            if (m_transcriptStore) {
              m_transcriptStore->updatePlanStatus(
                  m_scopedPlanId, QStringLiteral("failed"), reason);
            }

            emit planFailed(m_scopedPlanFilePath);

            tearDownScopedEditSession();
          });

  if (!m_scopedSession->executePlan(commands)) {
    TranscriptEvent event;
    event.type = TranscriptEvent::Type::Error;
    event.role = QStringLiteral("error");
    event.body = tr("Failed to resolve plan against the document.");
    appendEvent(event);

    emit planFailed(m_scopedPlanFilePath);

    tearDownScopedEditSession();
    return;
  }

  buildEditPlanEvent();

  if (m_transcriptStore) {
    m_transcriptStore->updatePlanStatus(
        m_scopedPlanId, QStringLiteral("generating"),
        tr("Generating edits…"));
  }

  emit planGenerationStarted(m_scopedPlanFilePath);

  m_generationEditIndex = 0;
  m_generatingEdit = false;

  startNextPendingEdit();
}

void OverseerWidget::buildEditPlanEvent() {
  QJsonArray commandArray;

  for (const EditCommand &command : m_scopedCommands) {
    commandArray.append(
        ChatWidgetSerialization::editCommandToJson(command));
  }

  TranscriptEvent event;
  event.type = TranscriptEvent::Type::EditPlan;
  event.role = QStringLiteral("plan");
  event.planId = m_scopedPlanId;
  event.planFilePath = m_scopedPlanFilePath;
  event.planInstruction = m_scopedInstruction;
  event.planCommands = commandArray;
  event.planStatus = QStringLiteral("pending");

  appendEvent(event);
}

void OverseerWidget::startNextPendingEdit() {
  if (!m_scopedSession) {
    finishPlanGeneration();
    return;
  }

  const auto &pending = m_scopedSession->pendingEdits();

  const PendingEdit *nextEdit = nullptr;

  for (const PendingEdit &edit : pending) {
    if (edit.completed)
      continue;

    nextEdit = &edit;
    break;
  }

  if (!nextEdit) {
    finishPlanGeneration();
    return;
  }

  const EditCommand &command = nextEdit->command;

  if (command.operation == EditCommand::Operation::Insert) {
    m_scopedSession->completeInsertFromInstruction(nextEdit->id);
    startNextPendingEdit();
    return;
  }

  if (!m_scopedSession->prepareStreaming(nextEdit->command, nextEdit->id)) {
    finishPlanGeneration();
    return;
  }

  m_generatingEdit = true;
  m_generationEditIndex = nextEdit->id;

  const bool isScopedReplacement =
      command.operation == EditCommand::Operation::Replace;

  const bool isScopeBodyReplacement =
      command.operation == EditCommand::Operation::ReplaceScope;

  QString systemPrompt;
  QString userPrompt;

  if (isScopedReplacement) {
    systemPrompt = QStringLiteral(
        "You are a text-substitution engine. The application will replace "
        "exactly one occurrence of a target substring in a document with "
        "your output.\n"
        "\n"
        "Rules:\n"
        "- Output ONLY the replacement for the target substring.\n"
        "- Output must be a single line. No newlines.\n"
        "- Do NOT include the target substring in your output.\n"
        "- Do NOT wrap your output in backticks, quotes, ---, or any "
        "other delimiter.\n"
        "- Do NOT explain your output.\n");
  } else if (isScopeBodyReplacement) {
    systemPrompt = QStringLiteral(
        "You are a text generator. The application will replace the "
        "entire body of a section or scope with your output.\n"
        "\n"
        "Rules:\n"
        "- Output ONLY the new body content for the scope.\n"
        "- Do NOT include the scope's heading or title line.\n"
        "- Do NOT wrap your output in backticks or fences.\n"
        "- Do NOT explain your output.\n"
        "- End your output with exactly one trailing newline.\n");
  } else {
    systemPrompt = QStringLiteral(
        "You are an automated text generator. Output only the requested "
        "content. No commentary. No markdown fences. No explanation.");
  }

  if (isScopedReplacement) {
    userPrompt =
        QStringLiteral("Target substring:\n%1\n\nInstruction:\n%2\n\n"
                       "Output the replacement now. Nothing else.")
            .arg(command.findString, command.instruction);
  } else if (isScopeBodyReplacement) {
    userPrompt =
        QStringLiteral("Target scope: %1\n\nInstruction:\n%2\n\n"
                       "Output the new body now.")
            .arg(command.scopeId, command.instruction);
  } else {
    userPrompt = QStringLiteral("Instruction:\n%1\n\nTarget scope: %2")
                     .arg(command.instruction, command.scopeId);
  }

  QJsonArray messages;

  messages.append(QJsonObject{
      {QStringLiteral("role"), QStringLiteral("system")},
      {QStringLiteral("content"), systemPrompt}});

  messages.append(QJsonObject{
      {QStringLiteral("role"), QStringLiteral("user")},
      {QStringLiteral("content"), userPrompt}});

  if (m_payloadLogger) {
    m_payloadLogger->log(
        QStringLiteral("OVERSEER_EDIT_GENERATION_REQUEST"),
        QStringLiteral("Plan: %1\nFile: %2\nEdit: %3\nOperation: %4\n\n"
                       "Messages:\n%5")
            .arg(m_scopedPlanId, m_scopedPlanFilePath)
            .arg(nextEdit->id)
            .arg(static_cast<int>(command.operation))
            .arg(formatMessagesForLog(messages)));
  }

  m_generationToken = m_inferenceService->sendChatRequest(
      messages, QString(), 0.7, 120000);
}

void OverseerWidget::onGenerationDelta(
    const InferenceService::RequestToken &token, const QString &text) {
  if (!m_generatingEdit || token != m_generationToken || !m_scopedSession)
    return;

  m_scopedSession->appendStreaming(text);
}

void OverseerWidget::onGenerationFinished(
    const InferenceService::RequestToken &token) {
  if (!m_generatingEdit || token != m_generationToken || !m_scopedSession)
    return;

  m_generationToken = InferenceService::RequestToken();
  m_generatingEdit = false;

  if (!m_scopedSession->finishStreaming()) {
    return;
  }

  startNextPendingEdit();
}

void OverseerWidget::onGenerationError(
    const InferenceService::RequestToken &token, const QString &error) {
  if (!m_generatingEdit || token != m_generationToken || !m_scopedSession)
    return;

  m_generationToken = InferenceService::RequestToken();
  m_generatingEdit = false;

  TranscriptEvent event;
  event.type = TranscriptEvent::Type::Error;
  event.role = QStringLiteral("error");
  event.body = tr("Edit generation failed: %1").arg(error);
  appendEvent(event);

  if (m_transcriptStore) {
    m_transcriptStore->updatePlanStatus(
        m_scopedPlanId, QStringLiteral("failed"), error);
  }

  emit planFailed(m_scopedPlanFilePath);

  tearDownScopedEditSession();
}

bool OverseerWidget::allPendingEditsCompleted() const {
  if (!m_scopedSession)
    return false;

  for (const PendingEdit &edit : m_scopedSession->pendingEdits()) {
    if (!edit.completed)
      return false;
  }

  return true;
}

void OverseerWidget::finishPlanGeneration() {
  m_generatingEdit = false;
  m_generationToken = InferenceService::RequestToken();

  if (!m_scopedSession)
    return;

  if (!allPendingEditsCompleted()) {
    if (m_transcriptStore) {
      m_transcriptStore->updatePlanStatus(
          m_scopedPlanId, QStringLiteral("failed"),
          tr("Some edits did not generate."));
    }

    emit planFailed(m_scopedPlanFilePath);

    tearDownScopedEditSession();
    return;
  }

  if (m_transcriptStore) {
    m_transcriptStore->updatePlanStatus(
        m_scopedPlanId, QStringLiteral("pending"),
        tr("Ready for review."));
  }

  emit planReviewReady(m_scopedPlanFilePath);

  if (!m_sessionSettings.effectiveAutoEdits())
    return;

  m_pendingPlanAwaitingAutoApply = true;

  QTimer::singleShot(0, this, [this]() { autoApplyPendingPlan(); });
}

void OverseerWidget::autoApplyPendingPlan() {
  if (!m_pendingPlanAwaitingAutoApply)
    return;

  if (!m_sessionSettings.effectiveAutoEdits()) {
    m_pendingPlanAwaitingAutoApply = false;
    return;
  }

  m_pendingPlanAwaitingAutoApply = false;

  if (!m_scopedSession)
    return;

  onPlanApplyRequested(m_scopedPlanId);
}

void OverseerWidget::onPlannerFailed(const QString &reason) {
  TranscriptEvent event;
  event.type = TranscriptEvent::Type::Error;
  event.role = QStringLiteral("error");
  event.body = tr("Planner failed: %1").arg(reason);

  appendEvent(event);

  emit planFailed(m_scopedPlanFilePath);

  tearDownScopedEditSession();
}

void OverseerWidget::tearDownScopedEditSession() {
  if (m_scopedPlanner) {
    m_scopedPlanner->abort();
    m_scopedPlanner->deleteLater();
    m_scopedPlanner = nullptr;
  }

  if (m_scopedSession) {
    m_scopedSession->abort();
    m_scopedSession->deleteLater();
    m_scopedSession = nullptr;
  }

  if (m_inferenceService && !m_generationToken.isNull()) {
    m_inferenceService->abortChatRequest(m_generationToken);
    m_generationToken = InferenceService::RequestToken();
  }

  m_generatingEdit = false;
  m_generationEditIndex = 0;
  m_pendingPlanAwaitingAutoApply = false;
}

void OverseerWidget::onPlanEditAccepted(const QString &planId, int editId) {
  if (planId != m_scopedPlanId || !m_scopedSession)
    return;

  m_scopedSession->acceptPendingEdit(editId);
}

void OverseerWidget::onPlanEditRejected(const QString &planId, int editId) {
  if (planId != m_scopedPlanId || !m_scopedSession)
    return;

  m_scopedSession->rejectPendingEdit(editId);
}

void OverseerWidget::onPlanApplyRequested(const QString &planId) {
  if (planId != m_scopedPlanId || !m_scopedSession)
    return;

  m_pendingPlanAwaitingAutoApply = false;

  if (m_transcriptStore) {
    m_transcriptStore->updatePlanStatus(planId, QStringLiteral("applying"),
                                        tr("Applying…"));
  }

  if (!m_scopedSession->applyAcceptedPendingEdits()) {
    if (m_transcriptStore) {
      m_transcriptStore->updatePlanStatus(
          planId, QStringLiteral("failed"), tr("Failed to apply edits."));
    }

    emit planFailed(m_scopedPlanFilePath);

    tearDownScopedEditSession();
    return;
  }

  if (!m_scopedPlanFilePath.isEmpty())
    emit saveWorkstationFileRequested(m_scopedPlanFilePath);

  if (m_transcriptStore) {
    m_transcriptStore->updatePlanStatus(planId, QStringLiteral("applied"),
                                        tr("Applied."));
  }

  emit planApplied(m_scopedPlanFilePath);

  tearDownScopedEditSession();
}

void OverseerWidget::onPlanCancelRequested(const QString &planId) {
  if (planId != m_scopedPlanId)
    return;

  m_pendingPlanAwaitingAutoApply = false;

  if (m_transcriptStore) {
    m_transcriptStore->updatePlanStatus(planId, QStringLiteral("cancelled"),
                                        tr("Cancelled by user."));
  }

  tearDownScopedEditSession();
}

void OverseerWidget::addOverviewReference(const QString &path) {
  if (path.isEmpty())
    return;

  addOverviewReferences({path});
}

void OverseerWidget::addOverviewReferences(const QStringList &paths) {
  if (!m_currentSession || paths.isEmpty())
    return;

  if (!m_sidePanel || !m_sidePanel->overviewPanel())
    return;

  const QString notesRoot = Settings::getRootDirectory();

  QStringList relatives;

  for (const QString &p : paths) {
    const QString rel = PathUtils::toRelative(p, notesRoot);
    if (!rel.isEmpty())
      relatives.append(rel);
  }

  if (relatives.isEmpty())
    return;

  m_sidePanel->overviewPanel()->addRelativePaths(relatives);
}

QString OverseerWidget::proposalsSidecarPath() const {
  if (!m_currentSession)
    return {};
  return QDir(m_currentSession->folderPath())
      .filePath(QStringLiteral("proposals.json"));
}

void OverseerWidget::loadProposals() {
  m_proposals.clear();

  const QString path = proposalsSidecarPath();
  if (path.isEmpty() || !QFileInfo::exists(path))
    return;

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    return;

  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  if (!doc.isArray())
    return;

  for (const QJsonValue &value : doc.array()) {
    if (!value.isObject())
      continue;

    const QJsonObject obj = value.toObject();

    MemoryProposal proposal;
    proposal.key = obj.value(QStringLiteral("key")).toString();
    proposal.fact = obj.value(QStringLiteral("fact")).toString();
    proposal.rationale = obj.value(QStringLiteral("rationale")).toString();
    proposal.status =
        obj.value(QStringLiteral("status")).toString(QStringLiteral("pending"));
    proposal.scope =
        obj.value(QStringLiteral("scope")).toString(QStringLiteral("global"));
    proposal.acceptedScope =
        obj.value(QStringLiteral("acceptedScope")).toString();

    if (proposal.key.isEmpty())
      continue;

    m_proposals.append(proposal);
  }
}

void OverseerWidget::saveProposals() {
  const QString path = proposalsSidecarPath();
  if (path.isEmpty())
    return;

  QJsonArray arr;

  for (const MemoryProposal &p : std::as_const(m_proposals)) {
    QJsonObject obj;
    obj.insert(QStringLiteral("key"), p.key);
    obj.insert(QStringLiteral("fact"), p.fact);
    obj.insert(QStringLiteral("rationale"), p.rationale);
    obj.insert(QStringLiteral("status"), p.status);
    obj.insert(QStringLiteral("scope"), p.scope);
    obj.insert(QStringLiteral("acceptedScope"), p.acceptedScope);
    arr.append(obj);
  }

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
    return;

  file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
}

QString OverseerWidget::recordProposal(const QString &fact,
                                       const QString &rationale,
                                       const QString &scope) {
  if (fact.isEmpty())
    return {};

  for (const MemoryProposal &p : std::as_const(m_proposals)) {
    if (p.fact == fact && p.scope == scope &&
        (p.status == QStringLiteral("pending") ||
         p.status == QStringLiteral("accepted"))) {
      return p.key;
    }
  }

  MemoryProposal proposal;
  proposal.fact = fact;
  proposal.rationale = rationale;
  proposal.status = QStringLiteral("pending");
  proposal.scope = scope;
  proposal.key = QString::number(
      qHash(fact + QChar('|') + rationale + QChar('|') + scope));

  m_proposals.append(proposal);

  saveProposals();
  return proposal.key;
}

void OverseerWidget::setProposalStatus(const QString &key,
                                       const QString &status,
                                       const QString &acceptedScope) {
  for (MemoryProposal &p : m_proposals) {
    if (p.key != key)
      continue;

    p.status = status;

    if (status == QStringLiteral("accepted")) {
      p.acceptedScope = acceptedScope;

      const QString targetPath =
          acceptedScope == QStringLiteral("session") && m_currentSession
              ? m_currentSession->memoryPath()
              : OverseerStorage::memoryPath();

      OverseerStorage::appendFactToMemoryFile(targetPath, p.fact);
    }

    break;
  }

  saveProposals();

  if (m_transcriptStore)
    m_transcriptStore->updateProposalStatus(key, status, acceptedScope);

  reloadMemoryPanels();
}

void OverseerWidget::reloadMemoryPanels() {
  if (!m_sidePanel)
    return;

  if (auto *mp = m_sidePanel->memoryPanel())
    mp->loadFromFile(OverseerStorage::memoryPath());

  if (m_currentSession) {
    if (auto *smp = m_sidePanel->sessionMemoryPanel())
      smp->loadFromFile(m_currentSession->memoryPath());
  }
}