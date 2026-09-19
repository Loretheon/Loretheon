#include "OverseerWidget.h"

#include "AutomationStrip.h"
#include "ChatWidgetSerialization.h"
#include "EditNoteReviewDialog.h"
#include "EditPlanner.h"
#include "EditSession.h"
#include "FileAgent.h"
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
#include "TranscriptPanel.h"
#include "TranscriptStore.h"
#include "Workstation.h"

#include "NotificationService.h"

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
#include <QRegularExpression>
#include <QSet>
#include <QSpinBox>
#include <QSplitter>
#include <QTextCursor>
#include <QTextDocument>
#include <QTextStream>
#include <QTimer>
#include <QUuid>
#include <QVBoxLayout>

#include <algorithm>

namespace {

constexpr auto ConductorActionSummaryFilename = "agents/conductor.json";

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

  m_queue = new ConductorQueue(this);
  m_roster = new ConductorRoster(this);

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

  connect(m_queue, &ConductorQueue::requestAdded, this,
          [this](const QString &) { drainQueue(); });

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
              if (token != m_activeConductorToken)
                return;

              m_conductorRawText += text;
            });

    connect(m_inferenceService, &InferenceService::llmFinished, this,
            [this](const InferenceService::RequestToken &token) {
              if (token != m_activeConductorToken)
                return;

              const QString requestId = m_activeRequestId;
              const QString raw = m_conductorRawText.trimmed();
              if (PayloadLogger *logger = sessionLogger()) {
                logger->log(
                    PayloadLogger::Subsystem::Conductor,
                    QStringLiteral("ROUTE_RESPONSE"),
                    QStringLiteral("Request: %1\n\nResponse:\n%2")
                        .arg(requestId, raw));
              }
              m_activeConductorToken = InferenceService::RequestToken();
              m_activeRequestId.clear();
              m_conductorRawText.clear();

              if (requestId.isEmpty()) {
                drainQueue();
                return;
              }

              QJsonParseError parseError;

              const QJsonDocument doc =
                  QJsonDocument::fromJson(raw.toUtf8(), &parseError);

              if (parseError.error != QJsonParseError::NoError ||
                  !doc.isObject()) {
                m_queue->setState(requestId, QStringLiteral("failed"));
                m_queue->setRejectReason(
                    requestId,
                    tr("Conductor produced invalid JSON: %1")
                        .arg(parseError.errorString()));

                drainQueue();
                return;
              }

              applyRoutingDecision(requestId, doc.object());

              drainQueue();
            });

    connect(m_inferenceService, &InferenceService::llmError, this,
            [this](const InferenceService::RequestToken &token,
                   const QString &error) {
              if (token != m_activeConductorToken)
                return;

              const QString requestId = m_activeRequestId;

              m_activeConductorToken = InferenceService::RequestToken();
              m_activeRequestId.clear();
              m_conductorRawText.clear();

              if (!requestId.isEmpty()) {
                m_queue->setState(requestId, QStringLiteral("failed"));
                m_queue->setRejectReason(requestId, error);
                appendActionSummary(
                    QStringLiteral("Routing failed for request %1: %2")
                        .arg(requestId, error));
              }

              drainQueue();
            });
  }

  rebuildSessionList();

  m_input->setEnabled(false);
  m_sendButton->setEnabled(false);
}

OverseerWidget::~OverseerWidget() = default;

PayloadLogger *OverseerWidget::sessionLogger() const {
  return m_sessionLogger ? m_sessionLogger : m_payloadLogger;
}

void OverseerWidget::setThemeTokens(const ThemeTokens &tokens) {
  Q_UNUSED(tokens);
}

void OverseerWidget::setWorkstation(Workstation *workstation) {
  m_workstation = workstation;
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

  m_queue->setQueuePath(
      QDir(session->folderPath()).filePath(QStringLiteral("queue.json")));

  m_roster->setRosterPath(
      QDir(session->folderPath()).filePath(QStringLiteral("roster.json")));

  m_dependencies.setPath(
      QDir(session->folderPath()).filePath(QStringLiteral("dependencies.dot")));

  m_sessionLogger = new PayloadLogger(session->logsPath(), this);

  m_actionSummary.clear();

  QFile summaryFile(
      QDir(session->folderPath()).filePath(ConductorActionSummaryFilename));

  if (summaryFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
    const QJsonDocument doc = QJsonDocument::fromJson(summaryFile.readAll());

    if (doc.isArray()) {
      for (const QJsonValue &v : doc.array()) {
        const QString line = v.toString();

        if (!line.isEmpty())
          m_actionSummary.append(line);
      }
    }
  }

  if (m_transcriptStore)
    m_transcriptStore->setSession(session);

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

  drainQueue();
}

void OverseerWidget::closeSession() {
  if (!m_currentSession)
    return;

  if (m_inferenceService && !m_activeConductorToken.isNull()) {
    m_inferenceService->abortChatRequest(m_activeConductorToken);
    m_activeConductorToken = InferenceService::RequestToken();
  }

  const QStringList planIds = m_scopedSessions.keys();

  for (const QString &planId : planIds)
    tearDownScopedSession(planId);

  const QStringList agentIds = m_fileAgents.keys();

  for (const QString &agentId : agentIds) {
    if (m_roster)
      m_roster->remove(agentId);

    FileAgent *agent = m_fileAgents.take(agentId);

    if (agent) {
      agent->deleteLater();
    }
  }

  m_taskToAgent.clear();
  m_writeOwner.clear();
  m_agentWritePaths.clear();
  m_routingRequestId.clear();

  disconnect(m_currentSession, nullptr, this, nullptr);

  m_currentSession->deleteLater();
  m_currentSession = nullptr;

  if (m_transcriptStore)
    m_transcriptStore->clear();

  m_sessionHeader->setText(tr("No session"));

  m_sessionSettings = SessionSettings();
  m_automationStrip->setSettings(m_sessionSettings);
  m_automationStrip->setEnabledState(false);

  if (m_sessionLogger) {
    m_sessionLogger->deleteLater();
    m_sessionLogger = nullptr;
  }

  m_proposals.clear();
  m_actionSummary.clear();

  m_input->setEnabled(false);
  m_sendButton->setEnabled(false);
}

void OverseerWidget::submitRequest(const QString &text) {
  if (!m_currentSession)
    return;

  const QString id = m_queue->enqueue(text);

  m_dependencies.addNode(id);

  TranscriptEvent event;
  event.type = TranscriptEvent::Type::UserMessage;
  event.role = QStringLiteral("user");
  event.body = text;
  appendEvent(event);

  Q_UNUSED(id);
}

void OverseerWidget::cancelRequest(const QString &requestId) {
  if (requestId.isEmpty())
    return;

  const ConductorRequest req = m_queue->byId(requestId);

  if (req.id.isEmpty())
    return;

  if (req.state == QStringLiteral("done") ||
      req.state == QStringLiteral("failed") ||
      req.state == QStringLiteral("rejected"))
    return;

  if (!req.planId.isEmpty())
    tearDownScopedSession(req.planId);

  if (!req.workerId.isEmpty()) {
    FileAgent *agent = fileAgentById(req.workerId);

    if (agent) {
      const QStringList taskIds = m_taskToAgent.keys();

      for (const QString &taskId : taskIds) {
        if (m_taskToAgent.value(taskId) != requestId)
          continue;

        agent->cancel(taskId);
        m_taskToAgent.remove(taskId);
        break;
      }
    }
  }

  m_queue->setRejectReason(requestId, tr("Cancelled by user."));
  m_queue->setState(requestId, QStringLiteral("rejected"));

  appendActionSummary(
      QStringLiteral("Request %1 cancelled by user.").arg(requestId));
}

void OverseerWidget::removeFailedRequest(const QString &requestId) {
  if (requestId.isEmpty())
    return;

  const ConductorRequest req = m_queue->byId(requestId);

  if (req.id.isEmpty())
    return;

  const QStringList dependents = m_dependencies.edgesFrom(requestId);

  QStringList affected;

  for (const QString &dep : dependents) {
    const ConductorRequest r = m_queue->byId(dep);

    if (r.id.isEmpty())
      continue;

    if (r.state == QStringLiteral("done") ||
        r.state == QStringLiteral("failed") ||
        r.state == QStringLiteral("rejected"))
      continue;

    affected.append(dep);
  }

  enum class Choice { Proceed, Cancel };

  Choice choice = Choice::Proceed;

  if (!affected.isEmpty()) {
    QMessageBox box(this);
    box.setIcon(QMessageBox::Question);
    box.setWindowTitle(tr("Remove request"));
    box.setText(tr("Remove request %1?").arg(requestId.left(8)));
    box.setInformativeText(
        tr("%n dependent request(s) are waiting on this one. What "
           "should happen to them?",
           "", affected.size()));
    QPushButton *proceed =
        box.addButton(tr("Let dependents proceed"), QMessageBox::AcceptRole);
    QPushButton *cancel =
        box.addButton(tr("Cancel dependents"), QMessageBox::DestructiveRole);
    box.setDefaultButton(proceed);
    box.exec();

    choice = (box.clickedButton() == cancel) ? Choice::Cancel
                                             : Choice::Proceed;
  }

  for (const QString &dep : std::as_const(affected)) {
    if (choice == Choice::Cancel) {
      m_queue->setRejectReason(
          dep, tr("Dependency %1 was removed.").arg(requestId.left(8)));
      m_queue->setState(dep, QStringLiteral("rejected"));
    }
  }

  if (choice == Choice::Proceed) {
    for (const QString &dep : std::as_const(affected))
      m_dependencies.removeEdge(requestId, dep);
  } else {
    for (const QString &dep : std::as_const(affected))
      m_dependencies.removeNode(dep);
  }

  m_dependencies.removeNode(requestId);

  m_queue->remove(requestId);

  appendActionSummary(
      QStringLiteral("Removed request %1 (dependents: %2).")
          .arg(requestId)
          .arg(choice == Choice::Proceed ? QStringLiteral("proceed")
                                         : QStringLiteral("cancelled")));

  drainQueue();
}

void OverseerWidget::drainQueue() {
  if (!m_currentSession)
    return;

  if (!m_activeConductorToken.isNull())
    return;

  if (!m_routingRequestId.isEmpty())
    return;

  const ConductorRequest req = m_queue->nextInbox();

  if (req.id.isEmpty())
    return;

  if (!dependenciesSatisfied(req.id))
    return;

  m_routingRequestId = req.id;
  m_queue->setState(req.id, QStringLiteral("routing"));

  QTimer::singleShot(0, this, [this, req]() {
    if (!m_currentSession) {
      m_routingRequestId.clear();
      return;
    }

    if (m_routingRequestId != req.id)
      return;

    routeRequest(req);
  });
}

void OverseerWidget::routeRequest(const ConductorRequest &request) {
  m_routingRequestId.clear();

  if (!m_inferenceService) {
    m_queue->setState(request.id, QStringLiteral("failed"));
    m_queue->setRejectReason(request.id,
                             tr("Inference service unavailable."));
    return;
  }

  const QString prompt = buildConductorPrompt(request);

  QJsonArray messages;

  messages.append(QJsonObject{
      {QStringLiteral("role"), QStringLiteral("system")},
      {QStringLiteral("content"), prompt}});

  if (PayloadLogger *logger = sessionLogger()) {
    logger->log(
        PayloadLogger::Subsystem::Conductor,
        QStringLiteral("ROUTE_REQUEST"),
        QStringLiteral("Request: %1\n\nPrompt:\n%2")
            .arg(request.id, prompt));
  }

  m_activeRequestId = request.id;

  m_activeConductorToken = m_inferenceService->sendChatRequest(
    messages, QString(), 0.2, 60000, QString(), QJsonObject(), QJsonArray(),
    m_currentSession ? m_currentSession->name() : QString());

  if (PayloadLogger *logger = sessionLogger()) {
    logger->log(
        PayloadLogger::Subsystem::Conductor,
        QStringLiteral("ROUTE_TOKEN"),
        QStringLiteral("Request: %1\nToken: %2")
            .arg(request.id, m_activeConductorToken.toString()));
  }
}

bool OverseerWidget::dependenciesSatisfied(const QString &requestId) const {
  const QStringList deps = m_dependencies.edgesTo(requestId);

  for (const QString &dep : deps) {
    const ConductorRequest r = m_queue->byId(dep);

    if (r.id.isEmpty()) {
      qWarning() << "[OverseerWidget] Dependency" << dep
                 << "of request" << requestId
                 << "is not in the queue; treating as satisfied.";
      continue;
    }

    if (r.state != QStringLiteral("done"))
      return false;
  }

  return true;
}

bool OverseerWidget::dependenciesBlocked(const QString &requestId) const {
  const QStringList deps = m_dependencies.edgesTo(requestId);

  for (const QString &dep : deps) {
    const ConductorRequest r = m_queue->byId(dep);

    if (r.id.isEmpty()) {
      qWarning() << "[OverseerWidget] Dependency" << dep
                 << "of request" << requestId
                 << "is not in the queue; ignoring for blocked check.";
      continue;
    }

    if (r.state == QStringLiteral("failed") ||
        r.state == QStringLiteral("rejected"))
      return true;
  }

  return false;
}

void OverseerWidget::failDependentsOf(const QString &requestId,
                                      const QString &reason) {
  if (requestId.isEmpty())
    return;

  const QStringList dependents = m_dependencies.edgesFrom(requestId);

  for (const QString &dep : dependents) {
    const ConductorRequest r = m_queue->byId(dep);

    if (r.id.isEmpty())
      continue;

    if (r.state == QStringLiteral("done") ||
        r.state == QStringLiteral("failed") ||
        r.state == QStringLiteral("rejected"))
      continue;

    m_queue->setState(dep, QStringLiteral("failed"));
    m_queue->setRejectReason(
        dep, tr("Dependency %1 failed: %2")
                 .arg(requestId.left(8), reason));

    appendActionSummary(
        QStringLiteral("Failed dependent request %1 because dependency "
                       "%2 failed.")
            .arg(dep, requestId));
  }
}

void OverseerWidget::recordEdges(const QString &requestId,
                                 const QStringList &dependencies) {
  for (const QString &dep : dependencies) {
    const QString trimmed = dep.trimmed();

    if (trimmed.isEmpty() || trimmed == requestId)
      continue;

    m_dependencies.addEdge(trimmed, requestId);
  }
}

QStringList OverseerWidget::pathsNamedByInstruction(
    const QString &instruction) const {
  QStringList result;

  if (instruction.isEmpty())
    return result;

  static const QRegularExpression pathRe(
      QStringLiteral(R"(([A-Za-z0-9_\-./]+\.[A-Za-z0-9_\-]+))"));

  auto it = pathRe.globalMatch(instruction);

  while (it.hasNext()) {
    const auto match = it.next();

    const QString candidate = match.captured(1);

    if (candidate.contains(QStringLiteral("://")))
      continue;

    if (!result.contains(candidate))
      result.append(candidate);
  }

  return result;
}

bool OverseerWidget::instructionTouchesClaim(const QString &instruction,
                                             QString *claimedPath) const {
  if (claimedPath)
    claimedPath->clear();

  if (instruction.isEmpty() || m_writeOwner.isEmpty())
    return false;

  QStringList claimed = m_writeOwner.keys();

  std::sort(claimed.begin(), claimed.end(),
            [](const QString &a, const QString &b) {
              return a.size() > b.size();
            });

  for (const QString &path : claimed) {
    if (instruction.contains(path, Qt::CaseInsensitive)) {
      if (claimedPath)
        *claimedPath = path;
      return true;
    }

    const QString base = QFileInfo(path).fileName();

    if (!base.isEmpty() &&
        instruction.contains(base, Qt::CaseInsensitive)) {
      if (claimedPath)
        *claimedPath = path;
      return true;
    }
  }

  return false;
}

void OverseerWidget::onFileWriteClaimed(const QString &agentId,
                                        const QString &taskId,
                                        const QString &relativePath) {
  Q_UNUSED(taskId);

  if (agentId.isEmpty() || relativePath.isEmpty())
    return;

  m_writeOwner.insert(relativePath, agentId);

  QStringList &paths = m_agentWritePaths[agentId];

  if (!paths.contains(relativePath))
    paths.append(relativePath);

  appendActionSummary(
      QStringLiteral("%1 is writing %2.").arg(agentId, relativePath));
}

void OverseerWidget::onFileWriteReleased(const QString &agentId,
                                         const QString &taskId,
                                         const QString &relativePath) {
  Q_UNUSED(taskId);

  if (relativePath.isEmpty())
    return;

  const QString owner = m_writeOwner.value(relativePath);

  if (owner == agentId)
    m_writeOwner.remove(relativePath);

  auto it = m_agentWritePaths.find(agentId);

  if (it != m_agentWritePaths.end()) {
    it.value().removeAll(relativePath);

    if (it.value().isEmpty())
      m_agentWritePaths.erase(it);
  }
}

QString OverseerWidget::writeOwnerForPath(const QString &relativePath) const {
  return m_writeOwner.value(relativePath);
}

QString OverseerWidget::claimedPathForInstruction(
    const QString &instruction) const {
  QString claimed;

  if (instructionTouchesClaim(instruction, &claimed))
    return claimed;

  return {};
}

bool OverseerWidget::instructionCollidesWithAgentClaims(
    const QString &instruction, const QString &agentId) const {
  if (instruction.isEmpty() || agentId.isEmpty())
    return false;

  const QStringList claimed = m_agentWritePaths.value(agentId);

  if (claimed.isEmpty())
    return false;

  QStringList sorted = claimed;

  std::sort(sorted.begin(), sorted.end(),
            [](const QString &a, const QString &b) {
              return a.size() > b.size();
            });

  for (const QString &path : sorted) {
    if (instruction.contains(path, Qt::CaseInsensitive))
      return true;

    const QString base = QFileInfo(path).fileName();

    if (!base.isEmpty() &&
        instruction.contains(base, Qt::CaseInsensitive))
      return true;
  }

  return false;
}
OverseerWidget::DispatchPlan OverseerWidget::decideDispatch(
    const QString &requestId, const QJsonObject &decision) const {
  DispatchPlan plan;

  const QString action = decision.value(QStringLiteral("action")).toString();
  const QString instruction =
      decision.value(QStringLiteral("instruction")).toString();

  const QJsonArray depsArray =
      decision.value(QStringLiteral("depends_on")).toArray();

  for (const QJsonValue &v : depsArray) {
    const QString dep = v.toString().trimmed();

    if (!dep.isEmpty() && dep != requestId)
      plan.dependencies.append(dep);
  }

  if (action == QStringLiteral("answer")) {
    plan.kind = DispatchPlan::Kind::Answer;
    plan.answer = decision.value(QStringLiteral("text")).toString();
    return plan;
  }

  if (action == QStringLiteral("reject")) {
    plan.kind = DispatchPlan::Kind::Reject;
    plan.reason = decision.value(QStringLiteral("reason")).toString();
    return plan;
  }

  // Rule 0: unmet dependency. Nothing dispatches until its inputs exist.
  if (!dependenciesSatisfied(requestId)) {
    plan.kind = DispatchPlan::Kind::Defer;
    plan.instruction = instruction;
    return plan;
  }

  // Rule 1: someone is writing a file this request names. That writer
  // gets the request, no matter what the conductor said.
  QString claimedPath;

  if (instructionTouchesClaim(instruction, &claimedPath)) {
    const QString owner = writeOwnerForPath(claimedPath);

    if (!owner.isEmpty()) {
      plan.kind = DispatchPlan::Kind::Redirect;
      plan.agentId = owner;
      plan.claimedPath = claimedPath;
      plan.instruction = instruction;
      return plan;
    }
  }

  // Scoped edits are their own path; they do not go through agents.
  if (action == QStringLiteral("spawn_scoped_edit")) {
    plan.kind = DispatchPlan::Kind::SpawnEdit;
    plan.filePath = decision.value(QStringLiteral("file")).toString();
    plan.instruction = instruction;
    return plan;
  }

  // Rule 2: expertise. Give the request to the agent whose track
  // record matches the files this request names. This covers both
  // dependencies and incidental knowledge with one check.
  FileAgent *expert = bestExpertForInstruction(instruction);

  if (expert) {
    plan.kind = DispatchPlan::Kind::Route;
    plan.agentId = expert->id();
    plan.instruction = instruction;
    return plan;
  }

  // Rule 3: no expertise signal. Honour the conductor's choice if it
  // named an existing agent; otherwise give it to the least-loaded
  // agent if any exist, rather than hiring someone new.
  if (action == QStringLiteral("route_to_worker")) {
    const QString agentId = decision.value(QStringLiteral("agent")).toString();

    if (fileAgentById(agentId)) {
      plan.kind = DispatchPlan::Kind::Route;
      plan.agentId = agentId;
      plan.instruction = instruction;
      return plan;
    }
  }

  FileAgent *anyAgent = leastLoadedAgent();

  if (anyAgent) {
    plan.kind = DispatchPlan::Kind::Route;
    plan.agentId = anyAgent->id();
    plan.instruction = instruction;
    return plan;
  }

  // Rule 4: no agents at all, or a genuinely new domain. Spawn.
  if (action == QStringLiteral("spawn_file_agent")) {
    plan.kind = DispatchPlan::Kind::SpawnAgent;
    plan.domain = decision.value(QStringLiteral("domain")).toString();
    plan.instruction = instruction;
    return plan;
  }

  plan.kind = DispatchPlan::Kind::Reject;
  plan.reason = QStringLiteral("Unknown conductor action: %1").arg(action);
  return plan;
}
void OverseerWidget::applyRoutingDecision(const QString &requestId,
                                          const QJsonObject &decision) {
  const DispatchPlan plan = decideDispatch(requestId, decision);

  recordEdges(requestId, plan.dependencies);

  if (plan.kind == DispatchPlan::Kind::Answer) {
    m_queue->setAnswer(requestId, plan.answer);
    m_queue->setState(requestId, QStringLiteral("done"));

    appendActionSummary(
        QStringLiteral("Answered request %1.").arg(requestId));

    TranscriptEvent event;
    event.type = TranscriptEvent::Type::AssistantMessage;
    event.role = QStringLiteral("assistant");
    event.body = plan.answer;
    appendEvent(event);

    return;
  }

  if (plan.kind == DispatchPlan::Kind::Reject) {
    m_queue->setRejectReason(requestId, plan.reason);
    m_queue->setState(requestId, QStringLiteral("rejected"));

    appendActionSummary(
        QStringLiteral("Rejected request %1: %2").arg(requestId, plan.reason));

    return;
  }

  if (plan.kind == DispatchPlan::Kind::Defer) {
    m_queue->setState(requestId, QStringLiteral("inbox"));

    appendActionSummary(
        QStringLiteral("Deferred request %1 until its dependencies "
                       "complete.").arg(requestId));

    return;
  }

  if (plan.kind == DispatchPlan::Kind::Redirect) {
    FileAgent *agent = fileAgentById(plan.agentId);

    if (!agent) {
      m_queue->setState(requestId, QStringLiteral("failed"));
      m_queue->setRejectReason(
          requestId,
          tr("Write owner %1 is no longer available.").arg(plan.agentId));
      return;
    }

    FileAgent::Task task;
    task.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    task.requestId = requestId;
    task.instruction = plan.instruction;

    m_taskToAgent.insert(task.id, requestId);

    agent->enqueue(task);

    m_queue->setWorker(requestId, plan.agentId, QString());
    m_queue->setState(requestId, QStringLiteral("delegated"));

    appendActionSummary(
        QStringLiteral("Redirected request %1 to %2 because %3 is "
                       "being written.")
            .arg(requestId, plan.agentId, plan.claimedPath));
    return;
  }

  if (plan.kind == DispatchPlan::Kind::SpawnEdit) {
    handleSpawnScopedEdit(requestId, plan.filePath, plan.instruction);
    return;
  }

  if (plan.kind == DispatchPlan::Kind::Route) {
    FileAgent *agent = fileAgentById(plan.agentId);

    if (!agent) {
      m_queue->setState(requestId, QStringLiteral("failed"));
      m_queue->setRejectReason(requestId,
                               tr("No such worker: %1").arg(plan.agentId));
      return;
    }

    FileAgent::Task task;
    task.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    task.requestId = requestId;
    task.instruction = plan.instruction;

    m_taskToAgent.insert(task.id, requestId);

    agent->enqueue(task);

    m_queue->setWorker(requestId, plan.agentId, QString());
    m_queue->setState(requestId, QStringLiteral("delegated"));

    appendActionSummary(
        QStringLiteral("Routed request %1 to %2.")
            .arg(requestId, plan.agentId));
    return;
  }

  // SpawnAgent. Only reached when no existing agent has any expertise
  // match and either the conductor named a new domain or there are no
  // agents yet. If the cap blocks the spawn, fall back to the
  // least-loaded existing agent rather than failing.
  const int cap = Settings::getOverseerFileAgentCap();

  if (m_fileAgents.size() >= cap) {
    FileAgent *fallback = leastLoadedAgent();

    if (!fallback) {
      m_queue->setState(requestId, QStringLiteral("failed"));
      m_queue->setRejectReason(
          requestId,
          tr("File agent cap reached and no agent is available."));
      return;
    }

    FileAgent::Task task;
    task.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    task.requestId = requestId;
    task.instruction = plan.instruction;

    m_taskToAgent.insert(task.id, requestId);

    fallback->enqueue(task);

    m_queue->setWorker(requestId, fallback->id(), QString());
    m_queue->setState(requestId, QStringLiteral("delegated"));

    appendActionSummary(
        QStringLiteral("File agent cap reached (%1 of %2); routed "
                       "request %3 to least-loaded %4.")
            .arg(m_fileAgents.size())
            .arg(cap)
            .arg(requestId, fallback->id()));
    return;
  }

  const QString agentId = spawnFileAgent(plan.domain);

  if (agentId.isEmpty()) {
    m_queue->setState(requestId, QStringLiteral("failed"));
    m_queue->setRejectReason(requestId,
                             tr("Could not spawn a file agent."));
    return;
  }

  FileAgent *agent = fileAgentById(agentId);

  if (!agent) {
    m_queue->setState(requestId, QStringLiteral("failed"));
    return;
  }

  FileAgent::Task task;
  task.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  task.requestId = requestId;
  task.instruction = plan.instruction;

  m_taskToAgent.insert(task.id, requestId);

  agent->enqueue(task);

  m_queue->setWorker(requestId, agentId, QString());
  m_queue->setState(requestId, QStringLiteral("delegated"));

  appendActionSummary(
      QStringLiteral("Spawned %1 for domain %2 (request %3).")
          .arg(agentId, plan.domain, requestId));
}

void OverseerWidget::handleSpawnScopedEdit(const QString &requestId,
                                           const QString &filePath,
                                           const QString &instruction,
                                           const QString &originAgentId,
                                           const QString &originTaskId) {
  if (!m_inferenceService || !m_focusedEditor) {
    m_queue->setState(requestId, QStringLiteral("failed"));
    m_queue->setRejectReason(
        requestId,
        tr("No editor available for the target file."));
    return;
  }

  const QString absolute = QDir(m_currentSession->outputPath())
                               .absoluteFilePath(filePath);

  if (m_workstation && m_workstation->isFileLocked(absolute)) {
    m_queue->setState(requestId, QStringLiteral("failed"));
    m_queue->setRejectReason(
        requestId,
        tr("A scoped edit is already active on %1.")
            .arg(QFileInfo(absolute).fileName()));
    return;
  }

  const int cap = Settings::getOverseerConcurrencyCap();

  if (m_scopedSessions.size() >= cap) {
    m_queue->setState(requestId, QStringLiteral("inbox"));
    return;
  }

  const QString planId = QUuid::createUuid().toString(QUuid::WithoutBraces);

  if (m_workstation)
    m_workstation->lockFile(absolute);

  ScopedSession ctx;
  ctx.planId = planId;
  ctx.requestId = requestId;
  ctx.filePath = absolute;
  ctx.instruction = instruction;
  ctx.originAgentId = originAgentId;
  ctx.originTaskId = originTaskId;

  auto *planner = new EditPlanner(m_inferenceService, this);

  connect(planner, &EditPlanner::planValidated, this,
          [this, planId](const QVector<EditCommand> &commands) {
            onPlannerValidated(planId, commands);
          });

  connect(planner, &EditPlanner::failed, this,
          [this, planId](const QString &reason) {
            onPlannerFailed(planId, reason);
          });

  ctx.planner = planner;

  m_scopedSessions.insert(planId, ctx);

  m_queue->setWorker(requestId, planId, planId);
  m_queue->setState(requestId, QStringLiteral("delegated"));

  planner->start(m_focusedEditor, instruction);
}

void OverseerWidget::onPlannerValidated(
    const QString &planId, const QVector<EditCommand> &commands) {
  auto it = m_scopedSessions.find(planId);

  if (it == m_scopedSessions.end())
    return;

  ScopedSession &ctx = it.value();

  if (commands.isEmpty()) {
    m_queue->setState(ctx.requestId, QStringLiteral("failed"));
    m_queue->setRejectReason(ctx.requestId,
                             tr("The planner returned no edits."));
    tearDownScopedSession(planId);
    return;
  }

  if (!m_focusedEditor) {
    m_queue->setState(ctx.requestId, QStringLiteral("failed"));
    m_queue->setRejectReason(ctx.requestId,
                             tr("No editor available."));
    tearDownScopedSession(planId);
    return;
  }

  ctx.commands = commands;
  ctx.session = new EditSession(m_focusedEditor, this);

  ctx.session->setSessionId(ctx.filePath);
  EditSession *session = ctx.session;

  session->setInferenceService(m_inferenceService);

  connect(session, &EditSession::pendingEditStarted, m_focusedEditor,
          &TextEdit::showPendingEdit);

  connect(session, &EditSession::pendingEditUpdated, m_focusedEditor,
          &TextEdit::updatePendingEdit);

  connect(session, &EditSession::pendingEditFinished, m_focusedEditor,
          [this](const PendingEdit &edit) {
            if (m_focusedEditor)
              m_focusedEditor->updatePendingEdit(edit);
          });

  connect(session, &EditSession::pendingEditsChanged, m_focusedEditor,
          &TextEdit::refreshPendingEdits);

  connect(session, &EditSession::failed, this,
          [this, planId](const QString &reason) {
            auto sessionIt = m_scopedSessions.find(planId);

            if (sessionIt == m_scopedSessions.end())
              return;

            const QString requestId = sessionIt->requestId;

            m_queue->setState(requestId, QStringLiteral("failed"));
            m_queue->setRejectReason(requestId, reason);

            tearDownScopedSession(planId);
          });

  connect(session, &EditSession::generationFinished, this,
          [this, planId](bool allCompleted) {
            auto sessionIt = m_scopedSessions.find(planId);

            if (sessionIt == m_scopedSessions.end())
              return;

            const QString requestId = sessionIt->requestId;

            if (allCompleted) {
              m_queue->setState(requestId, QStringLiteral("awaiting"));
            } else {
              m_queue->setState(requestId, QStringLiteral("failed"));
              m_queue->setRejectReason(
                  requestId, tr("Some edits did not generate."));
            }

            if (allCompleted && m_sessionSettings.effectiveAutoEdits())
              autoApplySession(planId);
          });

  if (!session->executePlan(commands)) {
    m_queue->setState(ctx.requestId, QStringLiteral("failed"));
    m_queue->setRejectReason(
        ctx.requestId,
        tr("Plan could not be resolved against the document."));
    tearDownScopedSession(planId);
    return;
  }

  buildEditPlanEvent(ctx);

  session->startAllPendingEdits();
}

void OverseerWidget::buildEditPlanEvent(const ScopedSession &ctx) {
  QJsonArray commandArray;

  for (const EditCommand &command : ctx.commands) {
    commandArray.append(
        ChatWidgetSerialization::editCommandToJson(command));
  }

  TranscriptEvent event;
  event.type = TranscriptEvent::Type::EditPlan;
  event.role = QStringLiteral("plan");
  event.planId = ctx.planId;
  event.planFilePath = ctx.filePath;
  event.planInstruction = ctx.instruction;
  event.planCommands = commandArray;
  event.planStatus = QStringLiteral("pending");

  appendEvent(event);
}

void OverseerWidget::autoApplySession(const QString &planId) {
  auto it = m_scopedSessions.find(planId);

  if (it == m_scopedSessions.end())
    return;

  if (!m_sessionSettings.effectiveAutoEdits())
    return;

  onPlanApplyRequested(planId);
}

void OverseerWidget::onPlannerFailed(const QString &planId,
                                     const QString &reason) {
  auto it = m_scopedSessions.find(planId);

  if (it == m_scopedSessions.end())
    return;

  const QString requestId = it->requestId;

  m_queue->setState(requestId, QStringLiteral("failed"));
  m_queue->setRejectReason(requestId, reason);

  appendActionSummary(
      QStringLiteral("Plan for %1 failed: %2").arg(requestId, reason));

  tearDownScopedSession(planId);
}

void OverseerWidget::tearDownScopedSession(const QString &planId) {
  auto it = m_scopedSessions.find(planId);

  if (it == m_scopedSessions.end())
    return;

  ScopedSession &ctx = it.value();

  const QString originAgentId = ctx.originAgentId;
  const QString originTaskId = ctx.originTaskId;

  if (ctx.planner) {
    ctx.planner->abort();
    ctx.planner->deleteLater();
    ctx.planner = nullptr;
  }

  if (ctx.session) {
    ctx.session->abort();
    ctx.session->deleteLater();
    ctx.session = nullptr;
  }

  if (m_workstation && !ctx.filePath.isEmpty())
    m_workstation->unlockFile(ctx.filePath);

  m_scopedSessions.erase(it);

  if (m_roster && !planId.isEmpty())
    m_roster->remove(planId);

  if (!originAgentId.isEmpty() && !originTaskId.isEmpty()) {
    FileAgent *agent = fileAgentById(originAgentId);

    if (agent) {
      agent->finishTask(originTaskId, true,
                        QStringLiteral("Scoped edit finished."));
    }
  }
}

void OverseerWidget::onPlanEditAccepted(const QString &planId, int editId) {
  auto it = m_scopedSessions.find(planId);

  if (it == m_scopedSessions.end() || !it->session)
    return;

  it->session->acceptPendingEdit(editId);
}

void OverseerWidget::onPlanEditRejected(const QString &planId, int editId) {
  auto it = m_scopedSessions.find(planId);

  if (it == m_scopedSessions.end() || !it->session)
    return;

  it->session->rejectPendingEdit(editId);
}

void OverseerWidget::onPlanApplyRequested(const QString &planId) {
  auto it = m_scopedSessions.find(planId);

  if (it == m_scopedSessions.end() || !it->session)
    return;

  const QString requestId = it->requestId;
  const QString filePath = it->filePath;

  if (!it->session->applyAcceptedPendingEdits()) {
    m_queue->setState(requestId, QStringLiteral("failed"));
    m_queue->setRejectReason(
        requestId, tr("Failed to apply edits."));

    tearDownScopedSession(planId);
    return;
  }

  if (!filePath.isEmpty())
    emit saveWorkstationFileRequested(filePath);

  m_queue->setState(requestId, QStringLiteral("done"));

  appendActionSummary(
      QStringLiteral("Applied %1 for request %2")
          .arg(QFileInfo(filePath).fileName(), requestId));

  emit planApplied(filePath);

  NotificationService::instance().info(
      tr("Applied"),
      tr("%1 has been updated.").arg(QFileInfo(filePath).fileName()),
      filePath);

  tearDownScopedSession(planId);
}

void OverseerWidget::onPlanCancelRequested(const QString &planId) {
  auto it = m_scopedSessions.find(planId);

  if (it == m_scopedSessions.end())
    return;

  const QString requestId = it->requestId;

  m_queue->setState(requestId, QStringLiteral("rejected"));
  m_queue->setRejectReason(requestId,
                           tr("Cancelled by user."));

  tearDownScopedSession(planId);
}

QString OverseerWidget::spawnFileAgent(const QString &domain) {
  // The cap is checked by the caller (applyRoutingDecision). This
  // function only constructs a new agent and returns its id; an empty
  // return means the construction failed, not that the cap was hit.
  //
  // The id comes from a monotonic counter, never from the map size,
  // so removing an agent can never cause a later spawn to collide
  // with an existing id.
  const QString id =
      QStringLiteral("agent-fs-%1").arg(m_nextAgentOrdinal++);

  auto *agent = new FileAgent(id, domain, m_inferenceService, &m_tools,
                              sessionLogger(), this);

  if (m_currentSession)
    agent->setOutputFolder(m_currentSession->outputPath());

  connect(agent, &FileAgent::taskFinished, this,
          [this](const QString &taskId, bool ok, const QString &result) {
            const QString requestId = m_taskToAgent.value(taskId);

            if (!requestId.isEmpty()) {
              if (!ok) {
                const ConductorRequest req = m_queue->byId(requestId);

                if (req.retryCount < 1) {
                  m_queue->retry(requestId);

                  appendActionSummary(
                      QStringLiteral("Retrying request %1 after failure.")
                          .arg(requestId));
                } else {
                  m_queue->setState(requestId, QStringLiteral("failed"));
                  m_queue->setRejectReason(requestId, result);

                  failDependentsOf(requestId, result);
                }
              } else {
                m_queue->setState(requestId, QStringLiteral("done"));
              }
            }

            m_taskToAgent.remove(taskId);
            syncRoster();

            drainQueue();
          });

  connect(agent, &FileAgent::delegateToScopedEdit, this,
          [this, id](const FileAgent::DelegateToScopedEdit &delegate) {
            handleSpawnScopedEdit(delegate.requestId, delegate.filePath,
                                  delegate.instruction, id,
                                  delegate.taskId);
          });

  connect(agent, &FileAgent::stateChanged, this,
          [this]() { syncRoster(); });

  connect(agent, &FileAgent::fileWriteClaimed, this,
          [this, id](const QString &taskId, const QString &relativePath) {
            onFileWriteClaimed(id, taskId, relativePath);
          });

  connect(agent, &FileAgent::fileWriteReleased, this,
          [this, id](const QString &taskId, const QString &relativePath) {
            onFileWriteReleased(id, taskId, relativePath);
          });

  m_fileAgents.insert(id, agent);

  syncRoster();

  return id;
}

FileAgent *OverseerWidget::fileAgentById(const QString &id) const {
  return m_fileAgents.value(id, nullptr);
}

void OverseerWidget::syncRoster() {
  if (!m_roster)
    return;

  const QVector<ConductorWorker> existing = m_roster->all();

  for (const ConductorWorker &w : existing) {
    if (w.type != QStringLiteral("file"))
      continue;

    if (!m_fileAgents.contains(w.id))
      m_roster->remove(w.id);
  }

  for (FileAgent *agent : std::as_const(m_fileAgents)) {
    if (!agent)
      continue;

    m_roster->add(agent->rosterEntry());
  }
}

QString OverseerWidget::buildConductorPrompt(
    const ConductorRequest &request) const {
  const QString globalMemory = OverseerStorage::readMemory();
  const QString sessionMemory =
      m_currentSession ? m_currentSession->memory() : QString();

  QString directoryHighlights;

  if (m_currentSession) {
    const QString output = m_currentSession->outputPath();

    QStringList topLevel;

    QDirIterator it(output, QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                    QDirIterator::NoIteratorFlags);

    while (it.hasNext() && topLevel.size() < 40) {
      const QString rel = QDir(output).relativeFilePath(it.next());

      if (rel == QStringLiteral("workstation.json"))
        continue;

      topLevel.append(rel);
    }

    topLevel.sort();

    directoryHighlights = topLevel.join(QChar('\n'));
  }

  if (directoryHighlights.isEmpty())
    directoryHighlights = QStringLiteral("(empty)");

  QString agents;

  if (m_fileAgents.isEmpty()) {
    agents = QStringLiteral("(no file agents yet)");
  } else {
    for (FileAgent *agent : std::as_const(m_fileAgents)) {
      if (!agent)
        continue;

      agents += agent->summaryForConductor();
    }
  }

  const int cap = Settings::getOverseerFileAgentCap();

  QString capacity;
  capacity += QStringLiteral("File agents: %1 of %2 in use.\n")
                  .arg(m_fileAgents.size())
                  .arg(cap);

  if (m_fileAgents.isEmpty()) {
    capacity += QStringLiteral("No file agents yet; spawning is "
                               "available.\n");
  } else if (m_fileAgents.size() >= cap) {
    capacity += QStringLiteral("At capacity. Do NOT use "
                               "spawn_file_agent. Use route_to_worker "
                               "for an existing agent, or reject with a "
                               "reason if no existing agent can serve "
                               "the request.\n");
  } else {
    capacity += QStringLiteral("Spawning is available for new "
                               "domains.\n");
  }

  QString writers;

  if (m_writeOwner.isEmpty()) {
    writers = QStringLiteral("(no writes in flight)");
  } else {
    QHash<QString, QStringList> byAgent;

    for (auto it = m_writeOwner.constBegin(); it != m_writeOwner.constEnd();
         ++it) {
      byAgent[it.value()].append(it.key());
    }

    for (auto it = byAgent.constBegin(); it != byAgent.constEnd(); ++it) {
      writers += QStringLiteral("%1 is writing: %2\n")
                     .arg(it.key(), it.value().join(QStringLiteral(", ")));
    }
  }

  QString actionHistory;

  if (m_actionSummary.isEmpty()) {
    actionHistory = QStringLiteral("(no recent actions)");
  } else {
    const int start = qMax(0, m_actionSummary.size() - 20);

    for (int i = start; i < m_actionSummary.size(); ++i)
      actionHistory += m_actionSummary.at(i) + QChar('\n');
  }

  QString depsSection;

  if (m_currentSession) {
    QSet<QString> doneSet;

    for (const ConductorRequest &r : m_queue->all()) {
      if (r.state == QStringLiteral("done"))
        doneSet.insert(r.id);
    }

    const QStringList ready = m_dependencies.ready(doneSet);

    if (!ready.isEmpty()) {
      depsSection = QStringLiteral("Ready to process:\n");

      for (const QString &id : ready)
        depsSection += QStringLiteral("  %1\n").arg(id);
    } else {
      depsSection = QStringLiteral("(no ready requests)");
    }

    QString edges;

    for (const QString &node : m_dependencies.nodes()) {
      const QStringList froms = m_dependencies.edgesTo(node);

      if (froms.isEmpty())
        continue;

      edges += QStringLiteral("%1 depends on %2\n")
                   .arg(node, froms.join(QStringLiteral(", ")));
    }

    if (edges.isEmpty())
      edges = QStringLiteral("(no dependencies recorded)");

    depsSection += QStringLiteral("\nEdges:\n%1").arg(edges);
  }

  const QString workingRoot =
      m_currentSession ? m_currentSession->outputPath() : QString();

  QString prompt;

  prompt += QStringLiteral(
      "You are the conductor for an Overseer session. You do not do\n"
      "work yourself. You look at one user request and decide which\n"
      "worker should handle it, or answer it directly, or reject it.\n"
      "\n"
      "The working directory for this session is:\n"
      "  %1\n"
      "All file paths in requests and in agent instructions are\n"
      "relative to this directory. Do NOT prefix paths with a domain\n"
      "name. A path like \"recipe.md\" means the file at the root of\n"
      "the working directory. A path like \"recipes/recipe.md\" means\n"
      "the file inside a subdirectory called recipes/.\n"
      "\n"
      "Respond with exactly one JSON object. No markdown fences. No\n"
      "explanatory text.\n"
      "\n"
      "The object has exactly one of these shapes:\n"
      "\n"
      "  {\"action\": \"answer\", \"text\": \"...\"}\n"
      "  {\"action\": \"reject\", \"reason\": \"...\"}\n"
      "  {\"action\": \"spawn_scoped_edit\",\n"
      "   \"file\": \"recipe.md\",\n"
      "   \"instruction\": \"...\"}\n"
      "  {\"action\": \"spawn_file_agent\",\n"
      "   \"domain\": \"recipes\",\n"
      "   \"instruction\": \"...\"}\n"
      "  {\"action\": \"route_to_worker\",\n"
      "   \"agent\": \"agent-fs-2\",\n"
      "   \"instruction\": \"...\"}\n"
      "\n"
      "Any shape may additionally carry a \"depends_on\" array of\n"
      "request ids this request needs completed first. Use it when the\n"
      "request builds on an earlier request's output. Use an empty\n"
      "array or omit it when the request stands alone.\n"
      "\n"
      "\"domain\" is a short label for a body of related work. It is\n"
      "NOT a file path and NOT a directory. Use a single word such as\n"
      "\"recipes\" or \"hastings\". Do not end it with a slash.\n"
      "\n"
      "Use \"answer\" for pure questions. Use \"reject\" when the\n"
      "request cannot be satisfied. Use \"spawn_scoped_edit\" when the\n"
      "request is a structural edit to a specific file that already\n"
      "exists. Use \"route_to_worker\" when an existing file agent\n"
      "already knows the files this request names. Use\n"
      "\"spawn_file_agent\" only when no existing agent has any\n"
      "knowledge of the files this request names.\n"
      "\n"
      "Do NOT use \"spawn_file_agent\" if the Agent capacity section\n"
      "below says you are at capacity. Use \"route_to_worker\" for an\n"
      "existing agent, or \"reject\" with a reason.\n"
      "\n"
      "## Global memory\n\n%2\n\n"
      "## Session memory\n\n%3\n\n"
      "## File directory highlights\n\n%4\n\n"
      "## Agent roster\n\n%5\n\n"
      "## Agent capacity\n\n%6\n\n"
      "## Writes in flight\n\n%7\n\n"
      "## Recent conductor actions\n\n%8\n\n"
      "## Dependency state\n\n%9\n\n"
      "## Current request\n\n%10\n")
      .arg(workingRoot, globalMemory, sessionMemory, directoryHighlights,
           agents, capacity, writers, actionHistory, depsSection,
           request.text);

  return prompt;
}

void OverseerWidget::appendActionSummary(const QString &line) {
  if (!m_currentSession)
    return;

  m_actionSummary.append(line);

  while (m_actionSummary.size() > 50)
    m_actionSummary.removeFirst();

  QJsonArray arr;

  for (const QString &entry : std::as_const(m_actionSummary))
    arr.append(entry);

  QFile file(QDir(m_currentSession->folderPath())
                 .filePath(ConductorActionSummaryFilename));

  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate |
                 QIODevice::Text))
    return;

  file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
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

int OverseerWidget::expertiseForInstruction(const QString &instruction,
                                            const QString &agentId) const {
  if (instruction.isEmpty() || agentId.isEmpty())
    return 0;

  FileAgent *agent = fileAgentById(agentId);

  if (!agent)
    return 0;

  const QStringList seen = agent->filesSeen();

  if (seen.isEmpty())
    return 0;

  int score = 0;

  // Longest paths first so that "notes/hastings/README.md" scores once
  // for the full path rather than once for the basename too.
  QStringList sorted = seen;

  std::sort(sorted.begin(), sorted.end(),
            [](const QString &a, const QString &b) {
              return a.size() > b.size();
            });

  for (const QString &path : sorted) {
    if (instruction.contains(path, Qt::CaseInsensitive)) {
      ++score;
      continue;
    }

    const QString base = QFileInfo(path).fileName();

    if (!base.isEmpty() &&
        instruction.contains(base, Qt::CaseInsensitive)) {
      ++score;
    }
  }

  return score;
}

FileAgent *OverseerWidget::bestExpertForInstruction(
    const QString &instruction) const {
  FileAgent *best = nullptr;
  int bestScore = 0;

  for (FileAgent *agent : m_fileAgents) {
    if (!agent)
      continue;

    const int score = expertiseForInstruction(instruction, agent->id());

    if (score == 0)
      continue;

    if (!best || score > bestScore) {
      best = agent;
      bestScore = score;
      continue;
    }

    if (score == bestScore) {
      if (agent->queueDepth() < best->queueDepth()) {
        best = agent;
        continue;
      }

      if (agent->queueDepth() == best->queueDepth() &&
          agent->id() < best->id()) {
        best = agent;
      }
    }
  }

  return best;
}

FileAgent *OverseerWidget::leastLoadedAgent() const {
  FileAgent *best = nullptr;

  for (FileAgent *agent : m_fileAgents) {
    if (!agent)
      continue;

    if (!best) {
      best = agent;
      continue;
    }

    if (agent->queueDepth() < best->queueDepth()) {
      best = agent;
      continue;
    }

    if (agent->queueDepth() == best->queueDepth() &&
        agent->id() < best->id()) {
      best = agent;
    }
  }

  return best;
}