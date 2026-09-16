#include "OverseerWidget.h"

#include "MemoryProposalCard.h"
#include "OverseerOverviewEditor.h"
#include "OverseerSession.h"
#include "OverseerStorage.h"
#include "OverseerTools.h"
#include "Settings.h"
#include "ToastStack.h"

#include "TextBrowser.h"
#include "inference/InferenceService.h"

#include <QComboBox>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontDatabase>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QSpinBox>
#include <QSplitter>
#include <QTabWidget>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextEdit>
#include <QVBoxLayout>

namespace {

QString humanReadableToolAction(const QString &toolName,
                                const QJsonObject &arguments,
                                const OverseerTool::Result &result) {
  const QString path = arguments.value(QStringLiteral("path")).toString();

  const QString verb = result.ok ? QStringLiteral("✓")
                                 : QStringLiteral("✗");

  if (toolName == QStringLiteral("write_file")) {
    return QStringLiteral("%1 Writing %2").arg(verb, path);
  }

  if (toolName == QStringLiteral("read_file") ||
      toolName == QStringLiteral("read_notes_file")) {
    return QStringLiteral("%1 Reading %2").arg(verb, path);
  }

  if (toolName == QStringLiteral("list_directory")) {
    const QString target = path.isEmpty() ? QStringLiteral("the output folder")
                                          : path;
    return QStringLiteral("%1 Listing %2").arg(verb, target);
  }

  if (toolName == QStringLiteral("create_directory")) {
    return QStringLiteral("%1 Creating %2").arg(verb, path);
  }

  if (toolName == QStringLiteral("propose_memory_fact")) {
    return QStringLiteral("%1 propose_memory_fact").arg(verb);
  }

  return QStringLiteral("%1 %2").arg(verb, toolName);
}

} // namespace

OverseerWidget::OverseerWidget(InferenceService *inferenceService,
                               QWidget *parent)
    : QWidget(parent), m_inferenceService(inferenceService) {
  OverseerStorage::ensureRoot();

  OverseerTools::installAll(m_tools);

  m_toolCallDepthLimit = Settings::getOverseerToolCallDepthLimit();

  // --- Left: session list -------------------------------------------------

  auto *leftPanel = new QWidget(this);
  auto *leftLayout = new QVBoxLayout(leftPanel);
  leftLayout->setContentsMargins(6, 6, 6, 6);
  leftLayout->setSpacing(6);

  auto *sessionsLabel = new QLabel(tr("Sessions"), leftPanel);
  QFont sessionsFont = sessionsLabel->font();
  sessionsFont.setBold(true);
  sessionsLabel->setFont(sessionsFont);
  leftLayout->addWidget(sessionsLabel);

  m_sessionList = new QListWidget(leftPanel);
  m_sessionList->setObjectName(QStringLiteral("overseerSessionList"));
  leftLayout->addWidget(m_sessionList, 1);

  m_newSessionButton = new QPushButton(tr("New Session"), leftPanel);
  leftLayout->addWidget(m_newSessionButton);

  auto *depthRow = new QHBoxLayout;
  auto *depthLabel = new QLabel(tr("Tool depth:"), leftPanel);
  m_toolCallDepthSpin = new QSpinBox(leftPanel);
  m_toolCallDepthSpin->setRange(1, 64);
  m_toolCallDepthSpin->setValue(m_toolCallDepthLimit);
  m_toolCallDepthSpin->setToolTip(
      tr("Maximum number of tool-call round trips per turn."));
  depthRow->addWidget(depthLabel);
  depthRow->addWidget(m_toolCallDepthSpin);
  depthRow->addStretch();
  leftLayout->addLayout(depthRow);

  // --- Center: transcript and input --------------------------------------

  auto *centerPanel = new QWidget(this);
  auto *centerLayout = new QVBoxLayout(centerPanel);
  centerLayout->setContentsMargins(6, 6, 6, 6);
  centerLayout->setSpacing(6);

  m_sessionHeader = new QLabel(tr("No session"), centerPanel);
  QFont headerFont = m_sessionHeader->font();
  headerFont.setBold(true);
  m_sessionHeader->setFont(headerFont);
  centerLayout->addWidget(m_sessionHeader);

  m_transcript = new TextBrowser(centerPanel);
  m_transcript->setOpenLinks(false);
  m_transcript->setOpenExternalLinks(false);
  m_transcript->setObjectName(QStringLiteral("overseerTranscript"));
  centerLayout->addWidget(m_transcript, 1);

  auto *inputRow = new QHBoxLayout;
  m_input = new QLineEdit(centerPanel);
  m_input->setPlaceholderText(tr("Ask Overseer…"));
  m_sendButton = new QPushButton(tr("Send"), centerPanel);
  inputRow->addWidget(m_input, 1);
  inputRow->addWidget(m_sendButton);
  centerLayout->addLayout(inputRow);

  // --- Right: memory, overview, tools log, user actions ------------------

  m_sideTabs = new QTabWidget(this);

  auto *memoryPage = new QWidget(m_sideTabs);
  auto *memoryLayout = new QVBoxLayout(memoryPage);
  memoryLayout->setContentsMargins(6, 6, 6, 6);
  memoryLayout->setSpacing(6);

  m_memoryEditor = new QPlainTextEdit(memoryPage);
  m_memoryEditor->setObjectName(QStringLiteral("overseerMemoryEditor"));
  m_memoryEditor->setAcceptDrops(false);
  memoryLayout->addWidget(m_memoryEditor, 1);

  m_saveMemoryButton = new QPushButton(tr("Save Memory"), memoryPage);
  memoryLayout->addWidget(m_saveMemoryButton);

  m_sideTabs->addTab(memoryPage, tr("Memory"));

  auto *overviewPage = new QWidget(m_sideTabs);
  auto *overviewLayout = new QVBoxLayout(overviewPage);
  overviewLayout->setContentsMargins(6, 6, 6, 6);
  overviewLayout->setSpacing(6);

  m_overviewEditor = new OverseerOverviewEditor(overviewPage);
  m_overviewEditor->setObjectName(QStringLiteral("overseerOverviewEditor"));
  overviewLayout->addWidget(m_overviewEditor, 1);

  auto *overviewButtons = new QHBoxLayout;
  m_refreshOverviewButton = new QPushButton(tr("Reload"), overviewPage);
  m_addOverviewButton = new QPushButton(tr("Add File…"), overviewPage);
  m_addOverviewButton->setToolTip(
      tr("Add a file that is not reachable from the tree "
         "(for example, a file outside the notes root)."));
  overviewButtons->addWidget(m_refreshOverviewButton);
  overviewButtons->addWidget(m_addOverviewButton);
  overviewLayout->addLayout(overviewButtons);

  m_sideTabs->addTab(overviewPage, tr("Overview"));

  m_toolLog = new QTextEdit(m_sideTabs);
  m_toolLog->setReadOnly(true);
  m_toolLog->setAcceptRichText(false);
  m_toolLog->setLineWrapMode(QTextEdit::NoWrap);
  m_toolLog->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  m_toolLog->setObjectName(QStringLiteral("overseerToolLog"));

  m_sideTabs->addTab(m_toolLog, tr("Tools"));

  // --- User actions needed tab -------------------------------------------

  m_userActionsPage = new QWidget(m_sideTabs);
  auto *userActionsLayout = new QVBoxLayout(m_userActionsPage);
  userActionsLayout->setContentsMargins(6, 6, 6, 6);
  userActionsLayout->setSpacing(6);

  m_userActionsScroll = new QScrollArea(m_userActionsPage);
  m_userActionsScroll->setWidgetResizable(true);
  m_userActionsScroll->setFrameShape(QFrame::NoFrame);

  m_userActionsContent = new QWidget;
  m_userActionsLayout = new QVBoxLayout(m_userActionsContent);
  m_userActionsLayout->setContentsMargins(0, 0, 0, 0);
  m_userActionsLayout->setSpacing(8);
  m_userActionsLayout->setAlignment(Qt::AlignTop);

  m_userActionsEmptyLabel =
      new QLabel(tr("Nothing needs your attention."), m_userActionsContent);
  m_userActionsEmptyLabel->setAlignment(Qt::AlignCenter);
  {
    QFont small = m_userActionsEmptyLabel->font();
    small.setItalic(true);
    m_userActionsEmptyLabel->setFont(small);
  }

  m_userActionsLayout->addWidget(m_userActionsEmptyLabel);
  m_userActionsLayout->addStretch(1);

  m_userActionsScroll->setWidget(m_userActionsContent);

  userActionsLayout->addWidget(m_userActionsScroll, 1);

  m_sideTabs->addTab(m_userActionsPage, tr("User actions needed"));

  // --- Toast stack (floating, top-right) ---------------------------------

  m_toastStack = new ToastStack(this);

  // --- Splitter ----------------------------------------------------------

  m_mainSplitter = new QSplitter(Qt::Horizontal, this);
  m_mainSplitter->addWidget(leftPanel);
  m_mainSplitter->addWidget(centerPanel);
  m_mainSplitter->addWidget(m_sideTabs);
  m_mainSplitter->setSizes({180, 640, 380});
  m_mainSplitter->setStretchFactor(0, 0);
  m_mainSplitter->setStretchFactor(1, 1);
  m_mainSplitter->setStretchFactor(2, 0);

  auto *rootLayout = new QVBoxLayout(this);
  rootLayout->setContentsMargins(0, 0, 0, 0);
  rootLayout->addWidget(m_mainSplitter);

  // --- Connections -------------------------------------------------------

  connect(m_newSessionButton, &QPushButton::clicked, this,
          &OverseerWidget::onNewSessionRequested);

  connect(m_sessionList, &QListWidget::itemSelectionChanged, this,
          &OverseerWidget::onSessionSelected);

  connect(m_sendButton, &QPushButton::clicked, this,
          &OverseerWidget::onSendClicked);

  connect(m_input, &QLineEdit::returnPressed, this,
          &OverseerWidget::onSendClicked);

  connect(m_saveMemoryButton, &QPushButton::clicked, this,
          &OverseerWidget::onSaveMemoryClicked);

  connect(m_refreshOverviewButton, &QPushButton::clicked, this,
          &OverseerWidget::onRefreshOverviewClicked);

  connect(m_addOverviewButton, &QPushButton::clicked, this,
          &OverseerWidget::onAddOverviewReferenceClicked);

  connect(m_overviewEditor, &OverseerOverviewEditor::filesDropped, this,
          &OverseerWidget::onOverviewFilesDropped);

  connect(m_toolCallDepthSpin,
          QOverload<int>::of(&QSpinBox::valueChanged), this,
          &OverseerWidget::onToolCallDepthChanged);

  if (m_inferenceService) {
    connect(m_inferenceService, &InferenceService::llmDelta, this,
            [this](const QString &text) {
              if (!m_expectingLlmResponse || !m_currentSession) {
                return;
              }
              appendAssistantChunk(text);
            });

    connect(m_inferenceService, &InferenceService::llmFinished, this, [this] {
      if (!m_expectingLlmResponse || !m_currentSession) {
        return;
      }
      m_expectingLlmResponse = false;
      finishAssistantBlock();
      m_turnMessages = QJsonArray();
      m_toolCallDepth = 0;
    });

    connect(m_inferenceService, &InferenceService::llmToolCalls, this,
            &OverseerWidget::onLlmToolCalls);

    connect(m_inferenceService, &InferenceService::llmError, this,
            [this](const QString &error) {
              if (!m_expectingLlmResponse || !m_currentSession) {
                return;
              }
              m_expectingLlmResponse = false;
              finishAssistantBlock();
              appendTranscriptEntry(tr("error"), error);
              m_turnMessages = QJsonArray();
              m_toolCallDepth = 0;
            });
  }

  rebuildSessionList();
  loadMemoryIntoEditor();
  rebuildUserActionsTab();

  m_input->setEnabled(false);
  m_sendButton->setEnabled(false);
}

void OverseerWidget::setThemeTokens(const ThemeTokens &tokens) {
  Q_UNUSED(tokens);
}

void OverseerWidget::rebuildSessionList() {
  m_sessionList->clear();

  const QStringList names = OverseerSession::list(OverseerStorage::rootPath());

  for (const QString &name : names) {
    m_sessionList->addItem(name);
  }
}

void OverseerWidget::onNewSessionRequested() {
  bool ok = false;

  const QString name = QInputDialog::getText(
      this, tr("New Overseer Session"), tr("Session name:"), QLineEdit::Normal,
      QString(), &ok);

  if (!ok) {
    return;
  }

  const QString trimmed = name.trimmed();

  if (trimmed.isEmpty()) {
    QMessageBox::warning(this, tr("New Overseer Session"),
                         tr("Session name cannot be empty."));
    return;
  }

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

  const QList<QListWidgetItem *> items =
      m_sessionList->findItems(trimmed, Qt::MatchExactly);

  if (!items.isEmpty()) {
    m_sessionList->setCurrentItem(items.first());
  }
}

void OverseerWidget::onSessionSelected() {
  const QList<QListWidgetItem *> selected = m_sessionList->selectedItems();

  if (selected.isEmpty()) {
    closeSession();
    return;
  }

  const QString name = selected.first()->text();

  OverseerSession *session =
      OverseerSession::open(OverseerStorage::rootPath(), name, this);

  if (!session) {
    QMessageBox::warning(this, tr("Open Overseer Session"),
                         tr("Could not open session '%1'.").arg(name));
    closeSession();
    return;
  }

  openSession(session);
}

void OverseerWidget::openSession(OverseerSession *session) {
  closeSession();

  m_currentSession = session;

  connect(m_currentSession, &OverseerSession::changed, this,
          &OverseerWidget::onSessionChangedExternally);

  m_sessionHeader->setText(tr("Session: %1").arg(session->name()));

  m_lastRenderedText.clear();
  loadProposals();
  renderTranscript();
  rebuildUserActionsTab();

  loadOverviewIntoEditor();

  m_input->setEnabled(true);
  m_sendButton->setEnabled(true);
}

void OverseerWidget::closeSession() {
  if (!m_currentSession) {
    return;
  }

  if (m_toastStack) {
    m_toastStack->dismissAll();
  }

  disconnect(m_currentSession, nullptr, this, nullptr);

  m_currentSession->deleteLater();
  m_currentSession = nullptr;

  m_sessionHeader->setText(tr("No session"));
  m_transcript->clear();
  m_overviewEditor->clear();

  m_lastRenderedText.clear();
  m_proposals.clear();
  m_turnMessages = QJsonArray();
  m_toolCallDepth = 0;

  rebuildUserActionsTab();

  m_input->setEnabled(false);
  m_sendButton->setEnabled(false);
}

void OverseerWidget::onSessionChangedExternally() {
  if (!m_currentSession) {
    return;
  }

  m_overviewEditor->setPlainText(m_currentSession->overview());
}

void OverseerWidget::loadMemoryIntoEditor() {
  m_memoryEditor->setPlainText(OverseerStorage::readMemory());
}

void OverseerWidget::loadOverviewIntoEditor() {
  if (!m_currentSession) {
    m_overviewEditor->clear();
    return;
  }

  m_overviewEditor->setPlainText(m_currentSession->overview());
}

void OverseerWidget::renderTranscript() {
  if (!m_currentSession) {
    m_transcript->clear();
    m_lastRenderedText.clear();
    return;
  }

  QString text = m_currentSession->transcript();

  if (!m_assistantRawText.isEmpty()) {
    if (!text.isEmpty() && !text.endsWith(QChar('\n'))) {
      text += QChar('\n');
    }
    text += QStringLiteral("## assistant\n");
    text += m_assistantRawText;
    text += QChar('\n');
  }

  if (text == m_lastRenderedText) {
    return;
  }

  m_lastRenderedText = text;

  m_transcript->setMarkdown(text);

  QTextCursor cursor = m_transcript->textCursor();
  cursor.movePosition(QTextCursor::End);
  m_transcript->setTextCursor(cursor);
  m_transcript->ensureCursorVisible();
}

void OverseerWidget::appendTranscriptEntry(const QString &role,
                                           const QString &text) {
  if (!m_currentSession) {
    return;
  }

  m_currentSession->appendTranscriptMessage(role, text);

  renderTranscript();
}

void OverseerWidget::appendAssistantChunk(const QString &text) {
  m_assistantRawText += text;
  renderTranscript();
}

void OverseerWidget::finishAssistantBlock() {
  if (!m_currentSession) {
    m_assistantRawText.clear();
    return;
  }

  const QString body = m_assistantRawText;
  m_assistantRawText.clear();

  if (!body.isEmpty()) {
    m_currentSession->appendTranscriptMessage(QStringLiteral("assistant"),
                                              body);
  }

  m_lastRenderedText.clear();
  renderTranscript();
}

QString OverseerWidget::buildSystemPrompt() const {
  const QString memory = OverseerStorage::readMemory();

  const QString overview =
      m_currentSession ? m_currentSession->overview() : QString();

  QString prompt;
  prompt += QStringLiteral(
      "You are Overseer, a persistent assistant that works across multiple "
      "sessions. You have access to a global Memory file and a per-session "
      "Overview of files the user has referenced.\n"
      "\n"
      "You have tools that let you list, read, and write files inside the "
      "current session's output folder. Any file you create must go through "
      "those tools; you cannot write anywhere else.\n"
      "\n"
      "You also have read_notes_file, which can read any file under the "
      "user's notes root. Use it for referenced files. It is read-only.\n"
      "\n"
      "You have propose_memory_fact. Use it sparingly for facts that should "
      "persist across sessions. The user will review each proposal and "
      "either accept or reject it.\n"
      "\n"
      "When your task is complete, reply with a short summary of what you "
      "did. Do not call more tools after that.\n\n");

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

  return context;
}

void OverseerWidget::dispatchChatRequest() {
  if (!m_inferenceService || !m_currentSession) {
    return;
  }

  m_expectingLlmResponse = true;

  m_inferenceService->sendChatRequest(m_turnMessages, QString(), 0.7, 120000,
                                      QString(), QJsonObject(),
                                      m_tools.schemas());
}

void OverseerWidget::logToolHumanReadable(
    const QString &toolName, const QJsonObject &arguments,
    const OverseerTool::Result &result) {
  const QString sentence = humanReadableToolAction(toolName, arguments, result);

  appendTranscriptEntry(QStringLiteral("tool"), sentence);
}

void OverseerWidget::logToolDetailed(const QString &toolName,
                                     const QJsonObject &arguments,
                                     const OverseerTool::Result &result,
                                     qint64 durationMs) {
  if (!m_toolLog) {
    return;
  }

  const QString timestamp =
      QDateTime::currentDateTime().toString(QStringLiteral("HH:mm:ss.zzz"));

  const QByteArray prettyArgs = QJsonDocument(arguments).toJson(
      QJsonDocument::Indented);

  QString entry;
  entry += QStringLiteral("[%1] %2 (%3 ms)\n")
               .arg(timestamp, toolName)
               .arg(durationMs);

  entry += QStringLiteral("  arguments:\n");

  const QStringList argLines = QString::fromUtf8(prettyArgs).split(
      QChar('\n'), Qt::SkipEmptyParts);

  for (const QString &line : argLines) {
    entry += QStringLiteral("    %1\n").arg(line);
  }

  entry += result.ok ? QStringLiteral("  result:\n")
                     : QStringLiteral("  error:\n");

  const QString body = result.ok ? result.output : result.error;

  const QStringList bodyLines = body.split(QChar('\n'));

  for (const QString &line : bodyLines) {
    entry += QStringLiteral("    %1\n").arg(line);
  }

  entry += QChar('\n');

  m_toolLog->moveCursor(QTextCursor::End);
  m_toolLog->insertPlainText(entry);
  m_toolLog->ensureCursorVisible();
}

void OverseerWidget::executeToolCalls(const QJsonArray &toolCalls) {
  const OverseerTool::Context context = currentToolContext();

  for (const QJsonValue &value : toolCalls) {
    if (!value.isObject()) {
      continue;
    }

    const QJsonObject call = value.toObject();

    const QString callId = call.value(QStringLiteral("id")).toString();

    const QJsonObject function =
        call.value(QStringLiteral("function")).toObject();

    const QString name =
        function.value(QStringLiteral("name")).toString();

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

    const qint64 durationMs =
        QDateTime::currentMSecsSinceEpoch() - started;

    logToolHumanReadable(name, arguments, result);

    logToolDetailed(name, arguments, result, durationMs);

    // If the tool was a memory proposal and it succeeded, record it,
    // append a readable line to the transcript, and spawn a toast.
    if (name == QStringLiteral("propose_memory_fact") && result.ok) {
      const QString fact =
          arguments.value(QStringLiteral("fact")).toString().trimmed();
      const QString rationale =
          arguments.value(QStringLiteral("rationale")).toString().trimmed();

      if (!fact.isEmpty()) {
        recordProposal(fact, rationale);

        appendTranscriptEntry(
            QStringLiteral("proposal"),
            tr("Memory proposal pending review — \"%1\".").arg(fact));

        if (m_toastStack) {
          m_toastStack->showProposalToast(fact, rationale);
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

    const QString content = result.ok ? result.output : result.error;

    QJsonObject toolMessage;
    toolMessage.insert(QStringLiteral("role"), QStringLiteral("tool"));
    toolMessage.insert(QStringLiteral("tool_call_id"), callId);
    toolMessage.insert(QStringLiteral("content"), content);

    m_turnMessages.append(toolMessage);
  }
}

void OverseerWidget::onLlmToolCalls(const QJsonArray &toolCalls) {
  if (!m_expectingLlmResponse || !m_currentSession) {
    return;
  }

  m_expectingLlmResponse = false;

  if (!m_assistantRawText.isEmpty()) {
    finishAssistantBlock();
  }

  bool onlyProposals = !toolCalls.isEmpty();

  for (const QJsonValue &value : toolCalls) {
    if (!value.isObject()) {
      onlyProposals = false;
      break;
    }

    const QJsonObject call = value.toObject();
    const QJsonObject function =
        call.value(QStringLiteral("function")).toObject();
    const QString name =
        function.value(QStringLiteral("name")).toString();

    if (name != QStringLiteral("propose_memory_fact")) {
      onlyProposals = false;
      break;
    }
  }

  if (!onlyProposals) {
    if (m_toolCallDepth >= m_toolCallDepthLimit) {
      appendTranscriptEntry(
          tr("error"),
          tr("Tool call depth limit reached (%1). Stopping.")
              .arg(m_toolCallDepthLimit));
      m_turnMessages = QJsonArray();
      m_toolCallDepth = 0;
      return;
    }

    ++m_toolCallDepth;
  }

  executeToolCalls(toolCalls);

  dispatchChatRequest();
}

void OverseerWidget::onSendClicked() {
  if (!m_currentSession || !m_inferenceService) {
    return;
  }

  const QString prompt = m_input->text().trimmed();

  if (prompt.isEmpty()) {
    return;
  }

  m_input->clear();

  appendTranscriptEntry(tr("user"), prompt);

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

void OverseerWidget::onSaveMemoryClicked() {
  if (!OverseerStorage::writeMemory(m_memoryEditor->toPlainText())) {
    QMessageBox::warning(this, tr("Save Memory"),
                         tr("Could not write the memory file."));
  }
}

void OverseerWidget::onRefreshOverviewClicked() {
  loadOverviewIntoEditor();
}

void OverseerWidget::onAddOverviewReferenceClicked() {
  if (!m_currentSession) {
    return;
  }

  const QString path = QFileDialog::getOpenFileName(
      this, tr("Add Overview Reference"), QString(), tr("All Files (*)"));

  if (path.isEmpty()) {
    return;
  }

  addOverviewReference(path);
}

void OverseerWidget::addOverviewReference(const QString &path) {
  if (path.isEmpty()) {
    return;
  }

  addOverviewReferences({path});
}

void OverseerWidget::addOverviewReferences(const QStringList &paths) {
  if (!m_currentSession || paths.isEmpty()) {
    return;
  }

  appendOverviewPaths(paths);
}

void OverseerWidget::onOverviewFilesDropped(const QStringList &paths) {
  addOverviewReferences(paths);
}

void OverseerWidget::appendOverviewPaths(const QStringList &paths) {
  if (!m_currentSession || paths.isEmpty()) {
    return;
  }

  QString current = m_overviewEditor->toPlainText();

  if (!current.isEmpty() && !current.endsWith(QChar('\n'))) {
    current += QChar('\n');
  }

  for (const QString &path : paths) {
    if (path.isEmpty()) {
      continue;
    }

    const QString line = QStringLiteral("- %1\n").arg(path);

    if (current.contains(line)) {
      continue;
    }

    current += line;
  }

  m_overviewEditor->setPlainText(current);
  m_currentSession->writeOverview(current);
}

void OverseerWidget::onToolCallDepthChanged(int value) {
  m_toolCallDepthLimit = qBound(1, value, 64);

  Settings::setOverseerToolCallDepthLimit(m_toolCallDepthLimit);
}

// ---------------------------------------------------------------------------
// Memory proposal plumbing
// ---------------------------------------------------------------------------

QString OverseerWidget::proposalsSidecarPath() const {
  if (!m_currentSession) {
    return {};
  }

  return QDir(m_currentSession->folderPath())
      .filePath(QStringLiteral("proposals.json"));
}

void OverseerWidget::loadProposals() {
  m_proposals.clear();

  const QString path = proposalsSidecarPath();

  if (path.isEmpty() || !QFileInfo::exists(path)) {
    return;
  }

  QFile file(path);

  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return;
  }

  const QByteArray raw = file.readAll();

  const QJsonDocument doc = QJsonDocument::fromJson(raw);

  if (!doc.isArray()) {
    return;
  }

  const QJsonArray arr = doc.array();

  for (const QJsonValue &value : arr) {
    if (!value.isObject()) {
      continue;
    }

    const QJsonObject obj = value.toObject();

    MemoryProposal proposal;
    proposal.key = obj.value(QStringLiteral("key")).toString();
    proposal.fact = obj.value(QStringLiteral("fact")).toString();
    proposal.rationale = obj.value(QStringLiteral("rationale")).toString();
    proposal.status =
        obj.value(QStringLiteral("status")).toString(QStringLiteral("pending"));

    if (proposal.key.isEmpty()) {
      continue;
    }

    m_proposals.append(proposal);
  }
}

void OverseerWidget::saveProposals() {
  const QString path = proposalsSidecarPath();

  if (path.isEmpty()) {
    return;
  }

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

  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate |
                 QIODevice::Text)) {
    return;
  }

  file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
}

QString OverseerWidget::recordProposal(const QString &fact,
                                       const QString &rationale) {
  if (fact.isEmpty()) {
    return {};
  }

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
  rebuildUserActionsTab();

  return proposal.key;
}

void OverseerWidget::setProposalStatus(const QString &key,
                                       const QString &status) {
  for (MemoryProposal &p : m_proposals) {
    if (p.key == key) {
      p.status = status;

      if (status == QStringLiteral("accepted")) {
        appendFactToMemory(p.fact);
      }

      break;
    }
  }

  saveProposals();
  rebuildUserActionsTab();
}

void OverseerWidget::appendFactToMemory(const QString &fact) {
  const QString trimmed = fact.trimmed();

  if (trimmed.isEmpty()) {
    return;
  }

  QString memory = OverseerStorage::readMemory();

  if (memory.isEmpty()) {
    memory = QStringLiteral("# Memory\n\n");
  }

  if (!memory.endsWith(QChar('\n'))) {
    memory += QChar('\n');
  }

  const QString sectionHeader =
      QStringLiteral("## Accepted proposals\n");

  if (!memory.contains(sectionHeader)) {
    memory += QChar('\n');
    memory += sectionHeader;
    memory += QChar('\n');
  }

  memory += QStringLiteral("- %1\n").arg(trimmed);

  OverseerStorage::writeMemory(memory);

  loadMemoryIntoEditor();
}

// ---------------------------------------------------------------------------
// User actions needed tab
// ---------------------------------------------------------------------------

void OverseerWidget::rebuildUserActionsTab() {
  if (!m_userActionsLayout) {
    return;
  }

  // Remove all existing cards (but keep the empty label and the stretch).
  const QList<MemoryProposalCard *> existing =
      m_userActionsContent->findChildren<MemoryProposalCard *>();

  for (MemoryProposalCard *card : existing) {
    m_userActionsLayout->removeWidget(card);
    card->deleteLater();
  }

  int pendingCount = 0;

  for (const MemoryProposal &p : std::as_const(m_proposals)) {
    if (p.status != QStringLiteral("pending")) {
      continue;
    }

    auto *card = new MemoryProposalCard(p.key, p.fact, p.rationale,
                                        m_userActionsContent);

    connect(card, &MemoryProposalCard::accepted, this,
            [this](const QString &key) {
              setProposalStatus(key, QStringLiteral("accepted"));
            });

    connect(card, &MemoryProposalCard::rejected, this,
            [this](const QString &key) {
              setProposalStatus(key, QStringLiteral("rejected"));
            });

    // Insert before the empty label so ordering is stable.
    m_userActionsLayout->insertWidget(pendingCount, card);
    ++pendingCount;
  }

  if (m_userActionsEmptyLabel) {
    m_userActionsEmptyLabel->setVisible(pendingCount == 0);
  }

  updateUserActionsTabTitle();
}

void OverseerWidget::updateUserActionsTabTitle() {
  if (!m_sideTabs || !m_userActionsPage) {
    return;
  }

  int pendingCount = 0;

  for (const MemoryProposal &p : std::as_const(m_proposals)) {
    if (p.status == QStringLiteral("pending")) {
      ++pendingCount;
    }
  }

  const int index = m_sideTabs->indexOf(m_userActionsPage);

  if (index < 0) {
    return;
  }

  if (pendingCount == 0) {
    m_sideTabs->setTabText(index, tr("User actions needed"));
  } else {
    m_sideTabs->setTabText(index,
                           tr("User actions needed (%1)").arg(pendingCount));
  }
}