#include "OverseerWidget.h"
#include "PathUtils.h"
#include "AutomationStrip.h"
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
  m_toolCallDepthSpin->setRange(1, 100000);
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

  // The side panel's User actions tab drives the exact same slots as
  // the transcript panel. Both views are projections of the runner's
  // source-of-truth lists, and both converge on the runner's mutators.
  connect(m_sidePanel, &OverseerSidePanel::memoryProposalAccepted, this,
          &OverseerWidget::onProposalAccepted);

  connect(m_sidePanel, &OverseerSidePanel::memoryProposalRejected, this,
          &OverseerWidget::onProposalRejected);

  connect(m_sidePanel, &OverseerSidePanel::editPlanApplyRequested, this,
          &OverseerWidget::onPlanApplyRequested);

  connect(m_sidePanel, &OverseerSidePanel::editPlanCancelRequested, this,
          &OverseerWidget::onPlanCancelRequested);

  connect(m_sidePanel, &OverseerSidePanel::editPlanOpenRequested, this,
          &OverseerWidget::focusPlanInTranscript);

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

  if (!m_boundRunner) {
    emit runnerBound(nullptr);
    return;
  }

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

  connect(m_boundRunner, &OverseerRunner::agentDepthLimitReached, this,
          &OverseerWidget::onAgentDepthLimitReached);

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

  // Populate the side panel from the runner's current state before
  // the runner has a chance to emit more changes.
  refreshUserActions();

  m_boundRunner->drainQueue();

  emit runnerBound(m_boundRunner);
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

  if (m_sidePanel) {
    m_sidePanel->setSession(nullptr);
    m_sidePanel->setPendingActions({});
  }

  m_sessionHeader->setText(tr("No session"));

  if (m_automationStrip) {
    m_automationStrip->setSettings(SessionSettings());
    m_automationStrip->setEnabledState(false);
  }

  m_input->setEnabled(false);
  m_sendButton->setEnabled(false);

  emit runnerBound(nullptr);
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
void OverseerWidget::retryRequest(const QString &requestId) {
  if (m_boundRunner)
    m_boundRunner->retryFailedRequest(requestId);
}

void OverseerWidget::skipRequest(const QString &requestId) {
  if (m_boundRunner)
    m_boundRunner->skipFailedRequest(requestId);
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
  const int clamped = qBound(1, value, 100000);
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
  if (!m_boundRunner)
    return;

  if (m_sidePanel)
    m_sidePanel->setSession(m_boundRunner->session());

  refreshUserActions();
}

void OverseerWidget::refreshUserActions() {
  if (!m_sidePanel)
    return;

  if (!m_boundRunner) {
    m_sidePanel->setPendingActions({});
    return;
  }

  m_sidePanel->setPendingActions(m_boundRunner->pendingActions());
}

void OverseerWidget::focusPlanInTranscript(const QString &planId) {
  if (planId.isEmpty())
    return;

  if (m_sidePanel)
    m_sidePanel->tabs()->setCurrentIndex(4); // Tools; UI does not
                                            // guarantee ordering, but
                                            // the transcript is not a
                                            // side-panel tab, so leave
                                            // the tab selection alone.

  Q_UNUSED(planId);
}

void OverseerWidget::onRunnerRequestFinished(
    const QString &sessionName, const QString &requestId, bool ok,
    const QString &summary, const QString &filePath) {
  Q_UNUSED(requestId);
  Q_UNUSED(filePath);

  const QString prefix = ok ? tr("Done") : tr("Failed");

  emit statusMessage(
      tr("%1 — %2: %3").arg(sessionName, prefix, summary), 4000);
}

void OverseerWidget::onAgentDepthLimitReached(const QString &agentId,
                                              int limit) {
  emit statusMessage(
      tr("Agent %1 stopped: exceeded its tool call depth limit (%2).")
          .arg(agentId)
          .arg(limit),
      6000);
}