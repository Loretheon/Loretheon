#include "../../include/overseer/OverseerWidget.h"

#include "../../include/overseer/OverseerSession.h"
#include "../../include/overseer/OverseerStorage.h"

#include "inference/InferenceService.h"

#include <QComboBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSplitter>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>

OverseerWidget::OverseerWidget(InferenceService *inferenceService,
                               QWidget *parent)
    : QWidget(parent), m_inferenceService(inferenceService) {
  OverseerStorage::ensureRoot();

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

  m_transcript = new QTextEdit(centerPanel);
  m_transcript->setReadOnly(true);
  m_transcript->setAcceptRichText(false);
  m_transcript->setLineWrapMode(QTextEdit::WidgetWidth);
  centerLayout->addWidget(m_transcript, 1);

  auto *inputRow = new QHBoxLayout;
  m_input = new QLineEdit(centerPanel);
  m_input->setPlaceholderText(tr("Ask Overseer…"));
  m_sendButton = new QPushButton(tr("Send"), centerPanel);
  inputRow->addWidget(m_input, 1);
  inputRow->addWidget(m_sendButton);
  centerLayout->addLayout(inputRow);

  // --- Right: memory and overview ----------------------------------------

  m_sideTabs = new QTabWidget(this);

  auto *memoryPage = new QWidget(m_sideTabs);
  auto *memoryLayout = new QVBoxLayout(memoryPage);
  memoryLayout->setContentsMargins(6, 6, 6, 6);
  memoryLayout->setSpacing(6);

  m_memoryEditor = new QPlainTextEdit(memoryPage);
  m_memoryEditor->setObjectName(QStringLiteral("overseerMemoryEditor"));
  memoryLayout->addWidget(m_memoryEditor, 1);

  m_saveMemoryButton = new QPushButton(tr("Save Memory"), memoryPage);
  memoryLayout->addWidget(m_saveMemoryButton);

  m_sideTabs->addTab(memoryPage, tr("Memory"));

  auto *overviewPage = new QWidget(m_sideTabs);
  auto *overviewLayout = new QVBoxLayout(overviewPage);
  overviewLayout->setContentsMargins(6, 6, 6, 6);
  overviewLayout->setSpacing(6);

  m_overviewEditor = new QPlainTextEdit(overviewPage);
  m_overviewEditor->setObjectName(QStringLiteral("overseerOverviewEditor"));
  overviewLayout->addWidget(m_overviewEditor, 1);

  auto *overviewButtons = new QHBoxLayout;
  m_refreshOverviewButton = new QPushButton(tr("Reload"), overviewPage);
  m_addOverviewButton = new QPushButton(tr("Add Reference"), overviewPage);
  overviewButtons->addWidget(m_refreshOverviewButton);
  overviewButtons->addWidget(m_addOverviewButton);
  overviewLayout->addLayout(overviewButtons);

  m_sideTabs->addTab(overviewPage, tr("Overview"));

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

  if (m_inferenceService) {
    connect(m_inferenceService, &InferenceService::llmDelta, this,
            [this](const QString &text) {
              if (!m_currentSession) {
                return;
              }
              QTextCursor cursor = m_transcript->textCursor();
              cursor.movePosition(QTextCursor::End);
              cursor.insertText(text);
              m_transcript->setTextCursor(cursor);
              m_transcript->ensureCursorVisible();
              m_assistantMessageOpen = true;
            });

    connect(m_inferenceService, &InferenceService::llmFinished, this, [this] {
      if (!m_currentSession) {
        return;
      }
      m_assistantMessageOpen = false;
    });

    connect(m_inferenceService, &InferenceService::llmError, this,
            [this](const QString &error) {
              if (!m_currentSession) {
                return;
              }
              appendTranscriptEntry(tr("error"), error);
            });
  }

  rebuildSessionList();
  loadMemoryIntoEditor();
}

void OverseerWidget::setThemeTokens(const ThemeTokens &tokens) {
  Q_UNUSED(tokens);
  // Styling is driven by the application-wide stylesheet. This hook is
  // present so that per-widget tweaks can be added later without changing
  // the class hierarchy.
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

  m_transcript->setPlainText(session->transcript());

  QTextCursor cursor = m_transcript->textCursor();
  cursor.movePosition(QTextCursor::End);
  m_transcript->setTextCursor(cursor);
  m_transcript->ensureCursorVisible();

  loadOverviewIntoEditor();

  m_input->setEnabled(true);
  m_sendButton->setEnabled(true);
}

void OverseerWidget::closeSession() {
  if (!m_currentSession) {
    return;
  }

  disconnect(m_currentSession, nullptr, this, nullptr);

  m_currentSession->deleteLater();
  m_currentSession = nullptr;

  m_sessionHeader->setText(tr("No session"));
  m_transcript->clear();
  m_overviewEditor->clear();

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

void OverseerWidget::appendTranscriptEntry(const QString &role,
                                           const QString &text) {
  if (!m_currentSession) {
    return;
  }

  m_currentSession->appendTranscriptMessage(role, text);

  QTextCursor cursor = m_transcript->textCursor();
  cursor.movePosition(QTextCursor::End);

  if (!m_transcript->toPlainText().isEmpty() && !cursor.atBlockStart()) {
    cursor.insertBlock();
  }

  cursor.insertText(QStringLiteral("## %1\n%2\n").arg(role, text));

  m_transcript->setTextCursor(cursor);
  m_transcript->ensureCursorVisible();
}

QString OverseerWidget::buildSystemPrompt() const {
  const QString memory = OverseerStorage::readMemory();

  const QString overview =
      m_currentSession ? m_currentSession->overview() : QString();

  QString prompt;
  prompt += QStringLiteral(
      "You are Overseer, a persistent assistant that works across multiple "
      "sessions. You have access to a global Memory file and a per-session "
      "Overview of files the user has referenced. You can read files listed "
      "in Overview and you can write files into your session's output "
      "directory only. You cannot modify files outside that directory.\n\n");

  prompt += QStringLiteral("## Global Memory\n\n%1\n\n").arg(memory);

  prompt += QStringLiteral("## Session Overview\n\n%1\n\n").arg(overview);

  return prompt;
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

  QJsonArray messages;

  messages.append(QJsonObject{
      {QStringLiteral("role"), QStringLiteral("system")},
      {QStringLiteral("content"), buildSystemPrompt()}});

  messages.append(QJsonObject{
      {QStringLiteral("role"), QStringLiteral("user")},
      {QStringLiteral("content"), prompt}});

  m_inferenceService->sendChatRequest(messages, QString(), 0.7, 120000);
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

  QString current = m_overviewEditor->toPlainText();

  if (!current.endsWith(QChar('\n'))) {
    current += QChar('\n');
  }

  current += QStringLiteral("- %1\n").arg(path);

  m_overviewEditor->setPlainText(current);
  m_currentSession->writeOverview(current);
}