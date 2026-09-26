#include "OverseerWidget.h"
#include "PathUtils.h"
#include "AutomationStrip.h"
#include "ConductorBoard.h"
#include "ConductorDock.h"
#include "ConductorQueue.h"
#include "ConductorRoster.h"
#include "DependencyGraph.h"
#include "OverseerRunner.h"
#include "OverseerSession.h"
#include "OverseerSessionList.h"
#include "OverseerSessionManager.h"
#include "OverseerSidePanel.h"
#include "OverviewPanel.h"
#include "Settings.h"
#include "TranscriptPanel.h"
#include "TranscriptStore.h"
#include "Workstation.h"

#include "TextEdit.h"

#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QToolButton>
#include <QVBoxLayout>

OverseerWidget::OverseerWidget(OverseerSessionManager *manager,
                               QWidget *parent)
    : QWidget(parent), m_manager(manager) {
  m_sessionListPanel = new OverseerSessionList(this);
  m_transcriptPanel = new TranscriptPanel(nullptr, this);
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
  m_input->setPlaceholderText(tr("Describe a task…"));
  m_sendButton = new QPushButton(tr("Send"), centerPanel);

  auto *depthLabel = new QLabel(tr("Tool depth:"), centerPanel);
  m_toolCallDepthSpin = new QSpinBox(centerPanel);
  m_toolCallDepthSpin->setRange(1, 64);
  m_toolCallDepthSpin->setValue(Settings::getOverseerToolCallDepthLimit());

  inputRow->addWidget(m_input, 1);
  inputRow->addWidget(m_sendButton);
  inputRow->addSpacing(12);
  inputRow->addWidget(depthLabel);
  inputRow->addWidget(m_toolCallDepthSpin);
  centerLayout->addLayout(inputRow);

  auto *rootLayout = new QVBoxLayout(this);
  rootLayout->setContentsMargins(0, 0, 0, 0);
  rootLayout->addWidget(centerPanel);

  connect(m_sessionListPanel, &OverseerSessionList::newSessionRequested, this,
          &OverseerWidget::onNewSessionRequested);

  connect(m_sessionListPanel, &OverseerSessionList::sessionSelected, this,
          &OverseerWidget::onSessionSelected);

  connect(m_sessionListPanel, &OverseerSessionList::sessionCleared, this,
          &OverseerWidget::onSessionCleared);

  connect(m_automationStrip, &AutomationStrip::settingsChanged, this,
          &OverseerWidget::onAutomationSettingsChanged);

  connect(m_sendButton, &QPushButton::clicked, this, [this]() {
    const QString text = m_input->text().trimmed();

    if (text.isEmpty())
      return;

    m_input->clear();
    submitRequest(text);
  });

  connect(m_input, &QLineEdit::returnPressed, this, [this]() {
    const QString text = m_input->text().trimmed();

    if (text.isEmpty())
      return;

    m_input->clear();
    submitRequest(text);
  });

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

  if (m_manager) {
    connect(m_manager, &OverseerSessionManager::sessionOpened, this,
            &OverseerWidget::onManagerSessionOpened);

    connect(m_manager, &OverseerSessionManager::sessionListChanged, this,
            &OverseerWidget::onManagerSessionListChanged);

    connect(m_manager, &OverseerSessionManager::requestFinished, this,
            &OverseerWidget::onRunnerRequestFinished);
  }

  rebuildSessionList();

  m_input->setEnabled(false);
  m_sendButton->setEnabled(false);
}

OverseerWidget::~OverseerWidget() = default;

void OverseerWidget::setThemeTokens(const ThemeTokens &tokens) {
  Q_UNUSED(tokens);
}

void OverseerWidget::setWorkstation(Workstation *workstation) {
  m_workstation = workstation;

  if (m_manager)
    m_manager->setWorkstation(workstation);
}

TranscriptStore *OverseerWidget::transcriptStore() const {
  return m_boundRunner ? m_boundRunner->transcriptStore() : nullptr;
}

ConductorQueue *OverseerWidget::queue() const {
  return m_boundRunner ? m_boundRunner->queue() : nullptr;
}

ConductorRoster *OverseerWidget::roster() const {
  return m_boundRunner ? m_boundRunner->roster() : nullptr;
}

DependencyGraph *OverseerWidget::dependencies() {
  return m_boundRunner ? m_boundRunner->dependencies() : nullptr;
}

void OverseerWidget::rebuildSessionList() {
  if (!m_manager)
    return;

  m_sessionListPanel->rebuild(m_manager->listSessions());
}

void OverseerWidget::bindToRunner(OverseerRunner *runner) {
  if (m_boundRunner == runner)
    return;

  unbindFromRunner(m_boundRunner);

  m_boundRunner = runner;

  if (!m_boundRunner)
    return;

  connect(m_boundRunner, &OverseerRunner::changed, this,
          &OverseerWidget::onRunnerChanged);

  connect(m_boundRunner, &OverseerRunner::fileWritten, this,
          &OverseerWidget::fileWritten);

  connect(m_boundRunner, &OverseerRunner::fileOpenRequested, this,
          &OverseerWidget::fileOpenRequested);

  connect(m_boundRunner, &OverseerRunner::fileCloseRequested, this,
          &OverseerWidget::fileCloseRequested);

  connect(m_boundRunner, &OverseerRunner::saveWorkstationFileRequested, this,
          &OverseerWidget::saveWorkstationFileRequested);

  connect(m_boundRunner, &OverseerRunner::planGenerationStarted, this,
          &OverseerWidget::planGenerationStarted);

  connect(m_boundRunner, &OverseerRunner::planReviewReady, this,
          &OverseerWidget::planReviewReady);

  connect(m_boundRunner, &OverseerRunner::planApplied, this,
          &OverseerWidget::planApplied);

  connect(m_boundRunner, &OverseerRunner::planFailed, this,
          &OverseerWidget::planFailed);

  if (m_transcriptPanel)
    m_transcriptPanel->setStore(m_boundRunner->transcriptStore());

  if (m_sidePanel)
    m_sidePanel->setSession(m_boundRunner->session());

  if (m_automationStrip) {
    m_automationStrip->setSettings(m_boundRunner->settings());
    m_automationStrip->setEnabledState(true);
  }

  if (m_workstation)
    m_boundRunner->setWorkstation(m_workstation);

  m_sessionHeader->setText(tr("Session: %1").arg(m_boundRunner->sessionName()));

  m_input->setEnabled(true);
  m_sendButton->setEnabled(true);

  m_boundRunner->drainQueue();
}

void OverseerWidget::unbindFromRunner(OverseerRunner *runner) {
  if (!runner)
    return;

  disconnect(runner, nullptr, this, nullptr);

  m_boundRunner = nullptr;
}

void OverseerWidget::openSessionByName(const QString &name) {
  onSessionSelected(name);
}

void OverseerWidget::onSessionSelected(const QString &name) {
  if (!m_manager)
    return;

  if (name.isEmpty()) {
    onSessionCleared();
    return;
  }

  OverseerRunner *runner = m_manager->openSession(name);

  if (!runner)
    return;

  m_activeSessionName = name;
  m_manager->setActiveSessionName(name);

  bindToRunner(runner);
}

void OverseerWidget::onSessionCleared() {
  m_activeSessionName.clear();

  if (m_manager)
    m_manager->setActiveSessionName(QString());

  unbindFromRunner(m_boundRunner);

  if (m_transcriptPanel)
    m_transcriptPanel->setStore(nullptr);

  if (m_sidePanel)
    m_sidePanel->setSession(nullptr);

  m_sessionHeader->setText(tr("No session"));

  if (m_automationStrip) {
    m_automationStrip->setSettings(SessionSettings());
    m_automationStrip->setEnabledState(false);
  }

  m_input->setEnabled(false);
  m_sendButton->setEnabled(false);
}

void OverseerWidget::onNewSessionRequested() {
  if (!m_manager)
    return;

  bool ok = false;

  const QString name = QInputDialog::getText(
      this, tr("New Overseer Session"), tr("Session name:"),
      QLineEdit::Normal, QString(), &ok);

  if (!ok)
    return;

  const QString trimmedName = name.trimmed();

  if (trimmedName.isEmpty())
    return;

  const QString description = QInputDialog::getText(
      this, tr("New Overseer Session"),
      tr("What is this session about? (one short sentence)"),
      QLineEdit::Normal, QString(), &ok);

  if (!ok)
    return;

  const QString trimmedDescription = description.trimmed();

  if (trimmedDescription.isEmpty()) {
    QMessageBox::warning(
        this, tr("New Overseer Session"),
        tr("A description is required. It is how the assistant knows "
           "what this session is for."));
    return;
  }

  if (!m_manager->createSession(trimmedName, trimmedDescription)) {
    QMessageBox::warning(
        this, tr("New Overseer Session"),
        tr("A session named '%1' already exists, or the name is "
           "invalid.").arg(trimmedName));
    return;
  }

  rebuildSessionList();
  m_sessionListPanel->selectByName(trimmedName);
}


void OverseerWidget::setFocusedFilePath(const QString &absolutePath) {
  if (m_boundRunner)
    m_boundRunner->setFocusedFilePath(absolutePath);
}

void OverseerWidget::setFocusedDocument(TextDocument *document,
                                        TextEdit *editor) {
  if (m_boundRunner)
    m_boundRunner->setFocusedDocument(document, editor);
}

QString OverseerWidget::submitRequest(const QString &text) {
  if (!m_boundRunner)
    return {};

  return m_boundRunner->submitRequest(text);
}

QString OverseerWidget::submitRequestFromLore(const QString &text) {
  if (!m_boundRunner)
    return {};

  return m_boundRunner->submitRequestFromLore(text);
}

void OverseerWidget::cancelRequest(const QString &requestId) {
  if (m_boundRunner)
    m_boundRunner->cancelRequest(requestId);
}

void OverseerWidget::removeFailedRequest(const QString &requestId) {
  if (m_boundRunner)
    m_boundRunner->removeFailedRequest(requestId);
}

void OverseerWidget::addOverviewReference(const QString &path) {
  if (path.isEmpty())
    return;

  addOverviewReferences({path});
}

void OverseerWidget::addOverviewReferences(const QStringList &paths) {
  if (!m_boundRunner || paths.isEmpty())
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

void OverseerWidget::onProposalAccepted(const QString &key,
                                        const QString &scope) {
  if (m_boundRunner)
    m_boundRunner->acceptProposal(key, scope);
}

void OverseerWidget::onProposalRejected(const QString &key) {
  if (m_boundRunner)
    m_boundRunner->rejectProposal(key);
}

void OverseerWidget::onPlanEditAccepted(const QString &planId, int editId) {
  if (m_boundRunner)
    m_boundRunner->acceptPlanEdit(planId, editId);
}

void OverseerWidget::onPlanEditRejected(const QString &planId, int editId) {
  if (m_boundRunner)
    m_boundRunner->rejectPlanEdit(planId, editId);
}

void OverseerWidget::onPlanApplyRequested(const QString &planId) {
  if (m_boundRunner)
    m_boundRunner->applyPlan(planId);
}

void OverseerWidget::onPlanCancelRequested(const QString &planId) {
  if (m_boundRunner)
    m_boundRunner->cancelPlan(planId);
}

void OverseerWidget::onAutomationSettingsChanged(
    const SessionSettings &settings) {
  if (m_boundRunner)
    m_boundRunner->setSessionSettings(settings);
}

void OverseerWidget::onToolCallDepthChanged(int value) {
  const int clamped = qBound(1, value, 64);
  Settings::setOverseerToolCallDepthLimit(clamped);

  if (m_boundRunner)
    m_boundRunner->setToolCallDepthLimit(clamped);
}

void OverseerWidget::onManagerSessionOpened(const QString &name) {
  if (name == m_activeSessionName) {
    bindToRunner(m_manager->runner(name));
  }
}

void OverseerWidget::onManagerSessionListChanged() {
  rebuildSessionList();
}

void OverseerWidget::onRunnerChanged() {
  // The view re-reads the queue, the roster, and the side panel from
  // the bound runner on demand. The signal is here so that future
  // changes (a live queue view, a live roster view) have a hook.
}

void OverseerWidget::onRunnerRequestFinished(const QString &sessionName,
                                             const QString &requestId,
                                             bool ok,
                                             const QString &summary,
                                             const QString &filePath) {
  Q_UNUSED(sessionName);
  Q_UNUSED(requestId);
  Q_UNUSED(ok);
  Q_UNUSED(summary);
  Q_UNUSED(filePath);
}

