#include "OverseerWidget.h"

#include "DirectoryExplorerSettings.h"
#include "EditNoteReviewDialog.h"
#include "MemoryPanel.h"
#include "MemoryProposalCard.h"
#include "OverseerSession.h"
#include "OverseerSessionList.h"
#include "OverseerSidePanel.h"
#include "OverseerStorage.h"
#include "OverseerTools.h"
#include "OverviewPanel.h"
#include "PathUtils.h"
#include "Settings.h"
#include "ToastStack.h"
#include "TranscriptPanel.h"
#include "TranscriptStore.h"

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
#include <QVBoxLayout>

OverseerWidget::OverseerWidget(InferenceService *inferenceService,
                               QWidget *parent)
    : QWidget(parent), m_inferenceService(inferenceService) {
  OverseerStorage::ensureRoot();


  OverseerTools::installAll(m_tools);

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
              if (token != m_activeToken || !m_currentSession)
                return;
              m_assistantRawText += text;
            });

    connect(m_inferenceService, &InferenceService::llmFinished, this,
            [this](const InferenceService::RequestToken &token) {
              if (token != m_activeToken)
                return;

              m_activeToken = InferenceService::RequestToken();

              if (!m_currentSession)
                return;

              if (!m_assistantRawText.isEmpty()) {
                TranscriptEvent event;
                event.type = TranscriptEvent::Type::AssistantMessage;
                event.role = QStringLiteral("assistant");
                event.body = m_assistantRawText;
                m_assistantRawText.clear();
                appendEvent(event);
              }

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

              handleToolCalls(toolCalls);
            });

    connect(m_inferenceService, &InferenceService::llmError, this,
            [this](const InferenceService::RequestToken &token,
                   const QString &error) {
              if (token != m_activeToken)
                return;

              m_activeToken = InferenceService::RequestToken();

              if (!m_currentSession)
                return;

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

void OverseerWidget::setThemeTokens(const ThemeTokens &tokens) {
  Q_UNUSED(tokens);
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

  if (m_transcriptStore) {
    m_transcriptStore->setSession(session);
  }

  if (m_sidePanel) {
    if (auto *mp = m_sidePanel->memoryPanel())
      mp->loadFromFile(OverseerStorage::memoryPath());

    if (auto *op = m_sidePanel->overviewPanel()) {
      op->setNotesRoot(Settings::getRootDirectory());
      op->loadFromFile(session->overviewPath());
    }
  }

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

  disconnect(m_currentSession, nullptr, this, nullptr);

  m_currentSession->deleteLater();
  m_currentSession = nullptr;

  if (m_transcriptStore)
    m_transcriptStore->clear();

  m_sessionHeader->setText(tr("No session"));

  m_proposals.clear();
  m_turnMessages = QJsonArray();
  m_toolCallDepth = 0;
  m_assistantRawText.clear();

  m_input->setEnabled(false);
  m_sendButton->setEnabled(false);
}

QString OverseerWidget::buildSystemPrompt() const {
  const QString memory = OverseerStorage::readMemory();
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
      "  - read_file(path) — read a file inside the output folder.\n"
      "  - write_file(path, content) — create or overwrite a file inside "
      "the output folder. Parent directories are created automatically.\n"
      "  - create_directory(path) — create a directory inside the output "
      "folder.\n"
      "  - open_file(path) — open a file from the output folder in the "
      "Workstation so the user can see it. Use this when you want to draw "
      "the user's attention to a specific file.\n"
      "  - close_file(path) — close a file that is open in the Workstation.\n"
      "  - read_notes_file(path) — read-only access to files under the "
      "user's notes root. Use this to consult user notes.\n"
      "  - edit_note(path, instruction) — open a copy of a notes-root file "
      "for the user to review and apply edits to. The original note is not "
      "modified until the user promotes it.\n"
      "  - propose_memory_fact(fact, rationale) — propose a durable fact "
      "for the user to accept into global memory.\n"
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
      "  - When your task is complete, reply with a short summary. Do not "
      "call more tools after that.\n"
      "\n")
      .arg(workspaceTree);



  prompt += QStringLiteral("## Global Memory\n\n%1\n\n").arg(memory);
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
    emit self->scopedEditRequested(editor, document, instruction);
  };

  return context;
}

void OverseerWidget::dispatchChatRequest() {
  if (!m_inferenceService || !m_currentSession)
    return;

  m_activeToken = m_inferenceService->sendChatRequest(
      m_turnMessages, QString(), 0.7, 120000, QString(), QJsonObject(),
      m_tools.schemas());
}

void OverseerWidget::handleToolCalls(const QJsonArray &toolCalls) {
  bool onlyNonBlocking = !toolCalls.isEmpty();

  for (const QJsonValue &value : toolCalls) {
    if (!value.isObject()) {
      onlyNonBlocking = false;
      break;
    }

    const QJsonObject call = value.toObject();
    const QJsonObject function =
        call.value(QStringLiteral("function")).toObject();
    const QString name = function.value(QStringLiteral("name")).toString();

    if (name != QStringLiteral("propose_memory_fact") &&
        name != QStringLiteral("edit_note")) {
      onlyNonBlocking = false;
      break;
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


    if (name == QStringLiteral("propose_memory_fact") && result.ok) {
      const QString fact =
          arguments.value(QStringLiteral("fact")).toString().trimmed();
      const QString rationale =
          arguments.value(QStringLiteral("rationale")).toString().trimmed();

      if (!fact.isEmpty()) {
        const QString key = recordProposal(fact, rationale);

        TranscriptEvent proposalEvent;
        proposalEvent.type = TranscriptEvent::Type::MemoryProposal;
        proposalEvent.role = QStringLiteral("proposal");
        proposalEvent.proposalKey = key;
        proposalEvent.proposalFact = fact;
        proposalEvent.proposalRationale = rationale;
        proposalEvent.proposalStatus = QStringLiteral("pending");
        appendEvent(proposalEvent);

        if (m_toastStack)
          m_toastStack->showProposalToast(fact, rationale);
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

void OverseerWidget::onProposalAccepted(const QString &key) {
  setProposalStatus(key, QStringLiteral("accepted"));
}

void OverseerWidget::onProposalRejected(const QString &key) {
  setProposalStatus(key, QStringLiteral("rejected"));
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
    arr.append(obj);
  }

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
    return;

  file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
}

QString OverseerWidget::recordProposal(const QString &fact,
                                       const QString &rationale) {
  if (fact.isEmpty())
    return {};

  for (const MemoryProposal &p : std::as_const(m_proposals)) {
    if (p.fact == fact &&
        (p.status == QStringLiteral("pending") ||
         p.status == QStringLiteral("accepted"))) {
      return p.key;
    }
  }

  MemoryProposal proposal;
  proposal.fact = fact;
  proposal.rationale = rationale;
  proposal.status = QStringLiteral("pending");
  proposal.key = QString::number(qHash(fact + QChar('|') + rationale));

  m_proposals.append(proposal);

  saveProposals();
  return proposal.key;
}

void OverseerWidget::setProposalStatus(const QString &key,
                                       const QString &status) {
  for (MemoryProposal &p : m_proposals) {
    if (p.key != key)
      continue;

    p.status = status;

    if (status == QStringLiteral("accepted"))
      appendFactToMemory(p.fact);

    break;
  }

  saveProposals();

  if (m_transcriptStore)
    m_transcriptStore->updateProposalStatus(key, status);

  if (m_sidePanel && m_sidePanel->memoryPanel())
    m_sidePanel->memoryPanel()->loadFromFile(OverseerStorage::memoryPath());
}

void OverseerWidget::appendFactToMemory(const QString &fact) {
  const QString trimmed = fact.trimmed();
  if (trimmed.isEmpty())
    return;

  QString memory = OverseerStorage::readMemory();
  if (memory.isEmpty())
    memory = QStringLiteral("# Memory\n\n");

  if (!memory.endsWith(QChar('\n')))
    memory += QChar('\n');

  const QString sectionHeader = QStringLiteral("## Accepted proposals\n");

  if (!memory.contains(sectionHeader)) {
    memory += QChar('\n');
    memory += sectionHeader;
    memory += QChar('\n');
  }

  memory += QStringLiteral("- %1\n").arg(trimmed);
  OverseerStorage::writeMemory(memory);
}

void OverseerWidget::setFocusedFilePath(const QString &absolutePath) {
  m_focusedFilePath = absolutePath;
}

void OverseerWidget::setFocusedDocument(TextDocument *document,
                                        TextEdit *editor) {
  m_focusedDocument = document;
  m_focusedEditor = editor;
}

