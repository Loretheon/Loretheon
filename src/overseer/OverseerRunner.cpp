#include "OverseerRunner.h"

#include "../../include/agent/tools/Tools.h"
#include "ChatWidgetSerialization.h"
#include "EditPlanner.h"
#include "EditSession.h"
#include "FileAgent.h"
#include "OverseerSession.h"
#include "OverseerStorage.h"
#include "PathUtils.h"
#include "PayloadLogger.h"
#include "PendingEdit.h"
#include "Settings.h"
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
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSet>
#include <QTimer>
#include <QUuid>

#include <algorithm>

namespace {

constexpr auto ConductorActionSummaryFilename = "agents/conductor.json";

QStringList parseFacts(const QString &memory) {
  QStringList facts;

  const QStringList lines = memory.split(QChar('\n'));

  bool inFacts = false;

  for (const QString &line : lines) {
    const QString trimmed = line.trimmed();

    if (trimmed.startsWith(QStringLiteral("## Accepted proposals"))) {
      inFacts = true;
      continue;
    }

    if (!inFacts)
      continue;

    if (trimmed.startsWith(QStringLiteral("## ")))
      break;

    if (trimmed.startsWith(QStringLiteral("- ")))
      facts.append(trimmed.mid(2).trimmed());
  }

  return facts;
}

QString normaliseFactKey(const QString &raw) {
  const QString trimmed = raw.trimmed();

  const int pipeIndex = trimmed.indexOf(QChar('|'));

  if (pipeIndex > 0)
    return trimmed.left(pipeIndex);

  return trimmed;
}

} // namespace

OverseerRunner::OverseerRunner(InferenceService *inferenceService,
                               const QString &sessionName,
                               QObject *parent)
    : QObject(parent), m_inferenceService(inferenceService),
      m_sessionName(sessionName) {
  OverseerStorage::ensureRoot();

  Tools::installAll(m_tools);

  m_payloadLogger = new PayloadLogger(this);

  m_toolCallDepthLimit = Settings::getOverseerToolCallDepthLimit();

  m_queue = new ConductorQueue(this);
  m_roster = new ConductorRoster(this);
  m_transcriptStore = new TranscriptStore(this);

  connect(m_queue, &ConductorQueue::requestAdded, this,
          [this](const QString &) {
            emit changed();
            drainQueue();
          });

  connect(m_queue, &ConductorQueue::requestChanged, this,
          [this](const QString &) { emit changed(); });

  connect(m_queue, &ConductorQueue::requestRemoved, this,
          [this](const QString &) { emit changed(); });

  connect(m_roster, &ConductorRoster::changed, this,
          [this]() { emit changed(); });

  connect(m_transcriptStore, &TranscriptStore::eventsReset, this,
          [this]() { emit changed(); });

  connect(m_transcriptStore, &TranscriptStore::eventAppended, this,
          [this](int) { emit changed(); });

  connect(m_transcriptStore, &TranscriptStore::eventUpdated, this,
          [this](int) { emit changed(); });

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
                m_conductorRetryInFlight = false;
                m_conductorRetryRaw.clear();
                drainQueue();
                return;
              }

              QJsonParseError parseError;
              const QJsonDocument doc =
                  QJsonDocument::fromJson(raw.toUtf8(), &parseError);

              if (parseError.error != QJsonParseError::NoError ||
                  !doc.isObject()) {
                if (!m_conductorRetryInFlight) {
                  m_conductorRetryInFlight = true;
                  m_conductorRetryRaw = raw;

                  if (PayloadLogger *logger = sessionLogger()) {
                    logger->log(
                        PayloadLogger::Subsystem::Conductor,
                        QStringLiteral("ROUTE_RETRY"),
                        QStringLiteral(
                            "Request: %1\nParse error: %2\nRetrying once.")
                            .arg(requestId, parseError.errorString()));
                  }

                  const ConductorRequest req = m_queue->byId(requestId);

                  if (!req.id.isEmpty()) {
                    QTimer::singleShot(0, this, [this, req]() {
                      routeRequest(req);
                    });
                    return;
                  }
                }

                const QString reason =
                    tr("Conductor produced invalid JSON: %1\n\n"
                       "Raw response:\n%2")
                        .arg(parseError.errorString(), raw);

                m_conductorRetryInFlight = false;
                m_conductorRetryRaw.clear();

                m_queue->setState(requestId, QStringLiteral("failed"));
                m_queue->setRejectReason(requestId, reason);

                announceRequestFinished(requestId, false, reason);

                drainQueue();
                return;
              }

              m_conductorRetryInFlight = false;
              m_conductorRetryRaw.clear();

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
              m_conductorRetryInFlight = false;
              m_conductorRetryRaw.clear();

              if (!requestId.isEmpty()) {
                m_queue->setState(requestId, QStringLiteral("failed"));
                m_queue->setRejectReason(requestId, error);

                appendActionSummary(
                    QStringLiteral("Routing failed for request %1: %2")
                        .arg(requestId, error));

                announceRequestFinished(requestId, false, error);
              }

              drainQueue();
            });
  }

  openSession();
}

OverseerRunner::~OverseerRunner() {
  closeSession();
}

void OverseerRunner::setWorkstation(Workstation *workstation) {
  m_workstation = workstation;
}

void OverseerRunner::setFocusedFilePath(const QString &absolutePath) {
  m_focusedFilePath = absolutePath;
}

void OverseerRunner::setFocusedDocument(TextDocument *document,
                                        TextEdit *editor) {
  m_focusedDocument = document;
  m_focusedEditor = editor;
}

void OverseerRunner::appendEvent(const TranscriptEvent &event) {
  if (m_transcriptStore)
    m_transcriptStore->append(event);
}

QString OverseerRunner::factKey(const QString &fact, const QString &scope) {
  return QString::number(qHash(fact + QChar('|') + scope));
}

QString OverseerRunner::factForKey(const QString &key) const {
  for (const MemoryProposal &p : m_proposals) {
    if (p.key == key)
      return p.fact;
  }

  return {};
}

QList<OverseerRunner::PendingAction> OverseerRunner::pendingActions() const {
  QList<PendingAction> result;

  for (const MemoryProposal &p : m_proposals) {
    if (p.status != QStringLiteral("pending"))
      continue;

    PendingAction action;
    action.kind = PendingAction::Kind::MemoryProposal;

    if (p.replaces.isEmpty()) {
      action.proposalMode = PendingAction::ProposalMode::NewFact;
    } else if (p.fact.isEmpty()) {
      action.proposalMode = PendingAction::ProposalMode::Delete;
    } else {
      action.proposalMode = PendingAction::ProposalMode::Replace;
    }

    action.key = p.key;
    action.title = p.fact;
    action.subtitle = p.rationale;
    action.replacedFact = p.replacedFact;
    action.sessionName = m_sessionName;
    action.scope = p.scope;
    result.append(action);
  }

  QStringList planIds = m_scopedSessions.keys();
  std::sort(planIds.begin(), planIds.end());

  for (const QString &planId : planIds) {
    const ScopedSession &ctx = m_scopedSessions.value(planId);

    if (!ctx.awaitingReview)
      continue;

    PendingAction action;
    action.kind = PendingAction::Kind::EditPlan;
    action.key = planId;
    action.title = QFileInfo(ctx.filePath).fileName();
    action.subtitle = tr("%n edit(s) pending review", "",
                         ctx.commands.size());
    action.sessionName = m_sessionName;
    result.append(action);
  }

  return result;
}

void OverseerRunner::openSession() {
  m_session = OverseerSession::open(OverseerStorage::rootPath(),
                                    m_sessionName, this);

  if (!m_session) {
    qWarning() << "[OverseerRunner] Session not found:" << m_sessionName;
    return;
  }

  SessionSettings::ensureFile(m_session->settingsPath());
  m_sessionSettings = SessionSettings::load(m_session->settingsPath());

  m_queue->setQueuePath(
      QDir(m_session->folderPath()).filePath(QStringLiteral("queue.json")));

  m_roster->setRosterPath(
      QDir(m_session->folderPath()).filePath(QStringLiteral("roster.json")));

  m_dependencies.setPath(
      QDir(m_session->folderPath()).filePath(QStringLiteral("dependencies.dot")));

  m_sessionLogger = new PayloadLogger(m_session->logsPath(), this);

  m_actionSummary.clear();

  QFile summaryFile(
      QDir(m_session->folderPath()).filePath(ConductorActionSummaryFilename));

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

  m_transcriptStore->setSession(m_session);

  loadProposals();

  drainQueue();
}

void OverseerRunner::closeSession() {
  if (!m_session)
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
    if (agent)
      agent->deleteLater();
  }

  m_taskToAgent.clear();
  m_writeOwner.clear();
  m_agentWritePaths.clear();
  m_routingRequestId.clear();

  m_transcriptStore->clear();

  if (m_sessionLogger) {
    m_sessionLogger->deleteLater();
    m_sessionLogger = nullptr;
  }

  m_proposals.clear();
  m_actionSummary.clear();
}

QString OverseerRunner::submitRequest(const QString &text) {
  if (!m_session)
    return {};

  const QString id = m_queue->enqueue(text);

  m_dependencies.addNode(id);

  TranscriptEvent event;
  event.type = TranscriptEvent::Type::UserMessage;
  event.origin = Origin::User;
  event.role = QStringLiteral("user");
  event.body = text;
  appendEvent(event);

  return id;
}

QString OverseerRunner::submitRequestFromLore(const QString &text) {
  if (!m_session)
    return {};

  const QString id = m_queue->enqueue(text);

  m_dependencies.addNode(id);

  TranscriptEvent event;
  event.type = TranscriptEvent::Type::UserMessage;
  event.origin = Origin::Lore;
  event.role = QStringLiteral("lore");
  event.body = text;
  appendEvent(event);

  return id;
}

void OverseerRunner::announceRequestFinished(const QString &requestId,
                                             bool ok,
                                             const QString &summary,
                                             const QString &filePath) {
  if (requestId.isEmpty())
    return;

  emit requestFinished(m_sessionName, requestId, ok, summary, filePath);
}

void OverseerRunner::cancelRequest(const QString &requestId) {
  if (requestId.isEmpty())
    return;

  const ConductorRequest req = m_queue->byId(requestId);

  if (req.id.isEmpty())
    return;

  const QVector<ConductorRequest> kids = m_queue->children(requestId);

  for (const ConductorRequest &child : kids) {
    cancelRequest(child.id);
  }

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

  announceRequestFinished(requestId, false, tr("Cancelled by user."));

  drainQueue();
}

void OverseerRunner::removeFailedRequest(const QString &requestId) {
  if (requestId.isEmpty())
    return;

  const ConductorRequest req = m_queue->byId(requestId);

  if (req.id.isEmpty())
    return;

  const QStringList removedChildren = m_queue->removeChildren(requestId);

  for (const QString &childId : removedChildren) {
    appendActionSummary(
        QStringLiteral("Removed child request %1.").arg(childId));
  }

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

  for (const QString &dep : std::as_const(affected)) {
    const QString reason =
        tr("Dependency %1 was removed.").arg(requestId.left(8));

    m_queue->setRejectReason(dep, reason);
    m_queue->setState(dep, QStringLiteral("rejected"));

    m_dependencies.removeNode(dep);

    announceRequestFinished(dep, false, reason);
  }

  m_dependencies.removeNode(requestId);
  m_queue->remove(requestId);

  appendActionSummary(
      QStringLiteral("Removed request %1.").arg(requestId));

  drainQueue();
}

void OverseerRunner::setSessionSettings(const SessionSettings &settings) {
  m_sessionSettings = settings;
  m_sessionSettings.normalize();

  if (m_session) {
    if (!m_sessionSettings.save(m_session->settingsPath())) {
      qWarning() << "[OverseerRunner] Failed to save session settings to"
                 << m_session->settingsPath();
    }
  }

  emit changed();
}

void OverseerRunner::settleDependentRequests() {
  const QVector<ConductorRequest> all = m_queue->all();

  for (const ConductorRequest &req : all) {
    if (req.state != QStringLiteral("inbox"))
      continue;

    if (!dependenciesBlocked(req.id))
      continue;

    const QStringList deps = m_dependencies.edgesTo(req.id);

    QString failedDep;

    for (const QString &dep : deps) {
      const ConductorRequest r = m_queue->byId(dep);

      if (r.id.isEmpty())
        continue;

      if (r.state == QStringLiteral("failed") ||
          r.state == QStringLiteral("rejected")) {
        failedDep = dep;
        break;
      }
    }

    const QString reason =
        tr("Dependency %1 did not complete.")
            .arg(failedDep.left(8));

    m_queue->setState(req.id, QStringLiteral("failed"));
    m_queue->setRejectReason(req.id, reason);

    appendActionSummary(
        QStringLiteral("Failed request %1 because dependency %2 did not "
                       "complete.")
            .arg(req.id, failedDep));

    announceRequestFinished(req.id, false, reason);
  }

  for (const ConductorRequest &req : m_queue->all()) {
    if (req.state != QStringLiteral("inbox"))
      continue;

    const QStringList unmet = unsatisfiedDependencies(req.id);

    if (!unmet.isEmpty()) {
      if (!req.deferred)
        m_queue->setDeferred(req.id, true);

      m_queue->setBlockedOn(req.id, unmet);
    } else {
      if (req.deferred)
        m_queue->setDeferred(req.id, false);

      if (!req.blockedOn.isEmpty())
        m_queue->setBlockedOn(req.id, {});
    }
  }
}

void OverseerRunner::failBlockedDependentsAfterTerminal(
    const QString &requestId) {
  if (requestId.isEmpty())
    return;

  const ConductorRequest req = m_queue->byId(requestId);

  if (req.id.isEmpty())
    return;

  if (req.state == QStringLiteral("failed") ||
      req.state == QStringLiteral("rejected")) {
    failDependentsOf(requestId, req.rejectReason);
  }

  settleDependentRequests();
}

void OverseerRunner::drainQueue() {
  if (!m_session)
    return;

  // Reload settings from disk on every drain. A user can toggle
  // auto-memory or auto-edits while the session is open; without this,
  // the runner keeps whatever was loaded at openSession time and
  // silently auto-accepts proposals the user believes are pending.
  m_sessionSettings = SessionSettings::load(m_session->settingsPath());
  m_sessionSettings.normalize();

  if (!m_activeConductorToken.isNull())
    return;

  if (!m_routingRequestId.isEmpty())
    return;

  settleDependentRequests();

  const ConductorRequest req = m_queue->nextReadyInbox();

  if (req.id.isEmpty())
    return;

  m_routingRequestId = req.id;
  m_queue->setState(req.id, QStringLiteral("routing"));

  QTimer::singleShot(0, this, [this, req]() {
    if (!m_session) {
      m_routingRequestId.clear();
      return;
    }

    if (m_routingRequestId != req.id)
      return;

    routeRequest(req);
  });
}

bool OverseerRunner::isPreDecidedAction(const QString &text) {
  const QString trimmed = text.trimmed();

  if (trimmed.isEmpty())
    return false;

  if (!trimmed.startsWith(QChar('{')))
    return false;

  QJsonParseError parseError;

  const QJsonDocument doc =
      QJsonDocument::fromJson(trimmed.toUtf8(), &parseError);

  if (parseError.error != QJsonParseError::NoError || !doc.isObject())
    return false;

  const QJsonObject obj = doc.object();

  return obj.contains(QStringLiteral("action"));
}

void OverseerRunner::routeRequest(const ConductorRequest &request) {
  m_routingRequestId.clear();

  if (isPreDecidedAction(request.text)) {
    const QJsonDocument doc =
        QJsonDocument::fromJson(request.text.trimmed().toUtf8());

    if (doc.isObject()) {
      if (PayloadLogger *logger = sessionLogger()) {
        logger->log(
            PayloadLogger::Subsystem::Conductor,
            QStringLiteral("PREDECIDED"),
            QStringLiteral("Request: %1\n\nAction:\n%2")
                .arg(request.id, request.text));
      }

      applyRoutingDecision(request.id, doc.object());
      return;
    }
  }

  if (!m_inferenceService) {
    const QString reason = tr("Inference service unavailable.");

    m_queue->setState(request.id, QStringLiteral("failed"));
    m_queue->setRejectReason(request.id, reason);

    announceRequestFinished(request.id, false, reason);
    return;
  }

  const QString prompt = buildConductorPrompt(request);

  QJsonArray messages;
  messages.append(QJsonObject{
      {QStringLiteral("role"), QStringLiteral("system")},
      {QStringLiteral("content"), prompt}});

  if (m_conductorRetryInFlight && !m_conductorRetryRaw.isEmpty()) {
    messages.append(QJsonObject{
        {QStringLiteral("role"), QStringLiteral("assistant")},
        {QStringLiteral("content"), m_conductorRetryRaw}});

    messages.append(QJsonObject{
        {QStringLiteral("role"), QStringLiteral("user")},
        {QStringLiteral("content"),
         QStringLiteral("Your previous response was not a single valid "
                        "JSON object. It must be exactly one JSON object "
                        "and nothing else. Reply again with the corrected "
                        "JSON object, and nothing else.")}});
  }

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
      m_session ? m_session->name() : QString());

  if (PayloadLogger *logger = sessionLogger()) {
    logger->log(
        PayloadLogger::Subsystem::Conductor,
        QStringLiteral("ROUTE_TOKEN"),
        QStringLiteral("Request: %1\nToken: %2")
            .arg(request.id, m_activeConductorToken.toString()));
  }
}

bool OverseerRunner::dependenciesSatisfied(const QString &requestId) const {
  const QStringList deps = m_dependencies.edgesTo(requestId);

  for (const QString &dep : deps) {
    const ConductorRequest r = m_queue->byId(dep);

    if (r.id.isEmpty()) {
      qWarning() << "[OverseerRunner] Dependency" << dep
                 << "of request" << requestId
                 << "is not in the queue; treating as satisfied.";
      continue;
    }

    if (r.state != QStringLiteral("done"))
      return false;
  }

  return true;
}

QStringList OverseerRunner::unsatisfiedDependencies(
    const QString &requestId) const {
  QStringList result;

  const QStringList deps = m_dependencies.edgesTo(requestId);

  for (const QString &dep : deps) {
    const ConductorRequest r = m_queue->byId(dep);

    if (r.id.isEmpty()) {
      continue;
    }

    if (r.state != QStringLiteral("done"))
      result.append(dep);
  }

  return result;
}

bool OverseerRunner::dependenciesBlocked(const QString &requestId) const {
  const QStringList deps = m_dependencies.edgesTo(requestId);

  for (const QString &dep : deps) {
    const ConductorRequest r = m_queue->byId(dep);

    if (r.id.isEmpty())
      continue;

    if (r.state == QStringLiteral("failed") ||
        r.state == QStringLiteral("rejected"))
      return true;
  }

  return false;
}

void OverseerRunner::failDependentsOf(const QString &requestId,
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

    announceRequestFinished(
        dep, false,
        tr("Dependency %1 failed: %2").arg(requestId.left(8), reason));
  }
}

void OverseerRunner::recordEdges(const QString &requestId,
                                 const QStringList &dependencies) {
  for (const QString &dep : dependencies) {
    const QString trimmed = dep.trimmed();

    if (trimmed.isEmpty() || trimmed == requestId)
      continue;

    m_dependencies.addEdge(trimmed, requestId);
  }
}

QStringList OverseerRunner::pathsNamedByInstruction(
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

bool OverseerRunner::instructionTouchesClaim(const QString &instruction,
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

void OverseerRunner::onFileWriteClaimed(const QString &agentId,
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

  emit changed();
}

void OverseerRunner::onFileWriteReleased(const QString &agentId,
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

  emit changed();
}

QString OverseerRunner::writeOwnerForPath(
    const QString &relativePath) const {
  return m_writeOwner.value(relativePath);
}

QString OverseerRunner::claimedPathForInstruction(
    const QString &instruction) const {
  QString claimed;

  if (instructionTouchesClaim(instruction, &claimed))
    return claimed;

  return {};
}

bool OverseerRunner::instructionCollidesWithAgentClaims(
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

OverseerRunner::DispatchPlan OverseerRunner::parseAction(
    const QString &requestId, const QJsonObject &action) const {
  DispatchPlan plan;

  const QString kind = action.value(QStringLiteral("action")).toString();
  const QString instruction =
      action.value(QStringLiteral("instruction")).toString();

  const QJsonArray depsArray =
      action.value(QStringLiteral("depends_on")).toArray();

  for (const QJsonValue &v : depsArray) {
    const QString dep = v.toString().trimmed();

    if (!dep.isEmpty() && dep != requestId)
      plan.dependencies.append(dep);
  }

  if (kind == QStringLiteral("answer")) {
    plan.kind = DispatchPlan::Kind::Answer;
    plan.answer = action.value(QStringLiteral("text")).toString();
    return plan;
  }

  if (kind == QStringLiteral("reject")) {
    plan.kind = DispatchPlan::Kind::Reject;
    plan.reason = action.value(QStringLiteral("reason")).toString();
    return plan;
  }

  if (kind == QStringLiteral("propose_memory")) {
    plan.kind = DispatchPlan::Kind::ProposeMemory;
    plan.memoryFact = action.value(QStringLiteral("fact")).toString();
    plan.memoryRationale =
        action.value(QStringLiteral("rationale")).toString();

    const QString scope = action.value(QStringLiteral("scope")).toString();
    plan.memoryScope =
        scope == QStringLiteral("session") ? QStringLiteral("session")
                                           : QStringLiteral("global");

    plan.memoryReplaces =
        action.value(QStringLiteral("replaces")).toString();

    return plan;
  }

  if (kind == QStringLiteral("spawn_scoped_edit")) {
    plan.kind = DispatchPlan::Kind::SpawnEdit;
    plan.filePath = action.value(QStringLiteral("file")).toString();
    plan.instruction = instruction;
    return plan;
  }

  if (kind == QStringLiteral("spawn_file_agent")) {
    plan.kind = DispatchPlan::Kind::SpawnAgent;
    plan.domain = action.value(QStringLiteral("domain")).toString();
    plan.instruction = instruction;
    return plan;
  }

  if (kind == QStringLiteral("route_to_worker")) {
    plan.kind = DispatchPlan::Kind::Route;
    plan.agentId = action.value(QStringLiteral("agent")).toString();
    plan.instruction = instruction;
    return plan;
  }

  plan.kind = DispatchPlan::Kind::Reject;
  plan.reason = QStringLiteral("Unknown conductor action: %1").arg(kind);
  return plan;
}

OverseerRunner::DispatchPlan OverseerRunner::decideDispatch(
    const QString &requestId, const QJsonObject &decision) const {
  const QString kind = decision.value(QStringLiteral("action")).toString();

  if (kind == QStringLiteral("batch")) {
    DispatchPlan batch;
    batch.kind = DispatchPlan::Kind::FanOut;

    const QJsonArray subActions =
        decision.value(QStringLiteral("actions")).toArray();

    for (const QJsonValue &v : subActions) {
      if (!v.isObject())
        continue;

      const QJsonObject obj = v.toObject();

      const QString subKind =
          obj.value(QStringLiteral("action")).toString();

      if (subKind == QStringLiteral("batch")) {
        const QJsonArray nested =
            obj.value(QStringLiteral("actions")).toArray();

        for (const QJsonValue &nv : nested) {
          if (nv.isObject())
            batch.fanOutActions.append(nv.toObject());
        }

        continue;
      }

      batch.fanOutActions.append(obj);
    }

    if (batch.fanOutActions.isEmpty()) {
      DispatchPlan reject;
      reject.kind = DispatchPlan::Kind::Reject;
      reject.reason = QStringLiteral("Empty batch.");
      return reject;
    }

    return batch;
  }

  DispatchPlan plan = parseAction(requestId, decision);

  if (plan.kind == DispatchPlan::Kind::Answer ||
      plan.kind == DispatchPlan::Kind::Reject ||
      plan.kind == DispatchPlan::Kind::ProposeMemory) {
    return plan;
  }

  if (!dependenciesSatisfied(requestId)) {
    DispatchPlan defer;
    defer.kind = DispatchPlan::Kind::Defer;
    defer.instruction = plan.instruction;
    return defer;
  }

  const QStringList namedPaths =
      pathsNamedByInstruction(plan.instruction);

  for (const QString &path : namedPaths) {
    const QString owner = writeOwnerForPath(path);

    if (owner.isEmpty())
      continue;

    DispatchPlan redirect;
    redirect.kind = DispatchPlan::Kind::Redirect;
    redirect.agentId = owner;
    redirect.claimedPath = path;
    redirect.instruction = plan.instruction;
    return redirect;
  }

  if (plan.kind == DispatchPlan::Kind::SpawnEdit)
    return plan;

  if (plan.kind == DispatchPlan::Kind::Route) {
    if (fileAgentById(plan.agentId))
      return plan;

    plan.agentId.clear();
  }

  FileAgent *expert = bestExpertForInstruction(plan.instruction);

  if (expert) {
    DispatchPlan route;
    route.kind = DispatchPlan::Kind::Route;
    route.agentId = expert->id();
    route.instruction = plan.instruction;
    return route;
  }

  if (plan.kind == DispatchPlan::Kind::SpawnAgent)
    return plan;

  FileAgent *anyAgent = leastLoadedAgent();

  if (anyAgent) {
    DispatchPlan route;
    route.kind = DispatchPlan::Kind::Route;
    route.agentId = anyAgent->id();
    route.instruction = plan.instruction;
    return route;
  }

  DispatchPlan reject;
  reject.kind = DispatchPlan::Kind::Reject;
  reject.reason = QStringLiteral("No worker available for this request.");
  return reject;
}

void OverseerRunner::applyRoutingDecision(const QString &requestId,
                                          const QJsonObject &decision) {
  const DispatchPlan plan = decideDispatch(requestId, decision);

  recordEdges(requestId, plan.dependencies);

  // A request whose declared dependencies are not yet satisfied must
  // not apply its plan. The answer text, or any other effect, was
  // generated against a state where the dependency was still pending
  // — for example, a fact that had been proposed but not accepted.
  // Put the request back in the inbox; when the dependencies resolve,
  // drainQueue re-routes it and the conductor sees the updated state.
  if (!plan.dependencies.isEmpty() && !dependenciesSatisfied(requestId)) {
    m_queue->setState(requestId, QStringLiteral("inbox"));

    appendActionSummary(
        QStringLiteral("Deferred request %1 until its declared "
                       "dependencies complete.")
            .arg(requestId));

    settleDependentRequests();
    drainQueue();
    return;
  }

  if (plan.kind == DispatchPlan::Kind::FanOut) {
    const ConductorRequest parent = m_queue->byId(requestId);

    const Origin origin =
        parent.id.isEmpty() ? Origin::User : parent.origin;

    QStringList childIds;

    for (const QJsonObject &action : plan.fanOutActions) {
      const QString childText = QString::fromUtf8(
          QJsonDocument(action).toJson(QJsonDocument::Compact));

      const QString childId =
          m_queue->enqueueChild(childText, requestId, origin);

      m_dependencies.addNode(childId);

      childIds.append(childId);
    }

    const QString summary =
        tr("Split into %n task(s).", "", childIds.size());

    m_queue->setAnswer(requestId, summary);
    m_queue->setState(requestId, QStringLiteral("done"));

    appendActionSummary(
        QStringLiteral("Fanned out request %1 into %2 child tasks.")
            .arg(requestId)
            .arg(childIds.size()));

    announceRequestFinished(requestId, true, summary);

    drainQueue();

    return;
  }

  applySinglePlan(requestId, plan);
}

void OverseerRunner::applySinglePlan(const QString &requestId,
                                     const DispatchPlan &plan) {
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

    announceRequestFinished(requestId, true, plan.answer);

    failBlockedDependentsAfterTerminal(requestId);
    return;
  }

  if (plan.kind == DispatchPlan::Kind::Reject) {
    m_queue->setRejectReason(requestId, plan.reason);
    m_queue->setState(requestId, QStringLiteral("rejected"));

    appendActionSummary(
        QStringLiteral("Rejected request %1: %2").arg(requestId, plan.reason));

    announceRequestFinished(requestId, false, plan.reason);

    failBlockedDependentsAfterTerminal(requestId);
    return;
  }

  if (plan.kind == DispatchPlan::Kind::ProposeMemory) {
    const bool isDelete = plan.memoryFact.trimmed().isEmpty() &&
                          !plan.memoryReplaces.isEmpty();

    if (plan.memoryFact.trimmed().isEmpty() && plan.memoryReplaces.isEmpty()) {
      const QString reason =
          tr("The conductor proposed a memory fact with no fact text "
             "and no fact to replace.");

      m_queue->setState(requestId, QStringLiteral("failed"));
      m_queue->setRejectReason(requestId, reason);

      announceRequestFinished(requestId, false, reason);

      failBlockedDependentsAfterTerminal(requestId);
      return;
    }

    const QString key = recordProposal(requestId,
                                       plan.memoryFact,
                                       plan.memoryRationale,
                                       plan.memoryScope,
                                       plan.memoryReplaces);

    if (key.isEmpty()) {
      const QString reason = tr("Could not record the memory proposal.");

      m_queue->setState(requestId, QStringLiteral("failed"));
      m_queue->setRejectReason(requestId, reason);

      announceRequestFinished(requestId, false, reason);

      failBlockedDependentsAfterTerminal(requestId);
      return;
    }

    const bool autoAccepted = m_sessionSettings.effectiveAutoMemory();

    QString replacedFact;

    for (const MemoryProposal &p : std::as_const(m_proposals)) {
      if (p.key == key) {
        replacedFact = p.replacedFact;
        break;
      }
    }

    TranscriptEvent event;
    event.type = TranscriptEvent::Type::MemoryProposal;
    event.role = QStringLiteral("memory");
    event.origin = Origin::User;
    event.body = plan.memoryFact;
    event.proposalKey = key;
    event.proposalFact = plan.memoryFact;
    event.proposalRationale = plan.memoryRationale;
    event.proposalScope = plan.memoryScope;
    event.proposalStatus =
        autoAccepted ? QStringLiteral("accepted") : QStringLiteral("pending");
    event.proposalAcceptedScope = autoAccepted ? plan.memoryScope : QString();
    event.proposalContext = replacedFact;
    appendEvent(event);

    QString summary;

    if (isDelete) {
      summary = autoAccepted
                    ? tr("Deleted a memory fact.")
                    : tr("Proposed deleting a memory fact.");
    } else if (!plan.memoryReplaces.isEmpty()) {
      summary = autoAccepted
                    ? tr("Replaced a memory fact.")
                    : tr("Proposed replacing a memory fact.");
    } else {
      summary = autoAccepted
                    ? tr("Remembered: %1").arg(plan.memoryFact)
                    : tr("Proposed a memory fact: %1").arg(plan.memoryFact);
    }

    m_queue->setAnswer(requestId, summary);

    // A pending proposal leaves the request in "awaiting": the user
    // has to accept or reject before the request's effect is real.
    // Dependents that declared depends_on this request stay blocked
    // until then. An auto-accepted proposal is already real.
    m_queue->setState(requestId,
                      autoAccepted ? QStringLiteral("done")
                                   : QStringLiteral("awaiting"));

    appendActionSummary(
        QStringLiteral("Recorded memory proposal %1 for request %2.")
            .arg(key, requestId));

    if (!autoAccepted) {
      QString notifBody;

      if (isDelete)
        notifBody = tr("Delete: %1").arg(replacedFact);
      else if (!plan.memoryReplaces.isEmpty())
        notifBody = tr("Replace \"%1\" with \"%2\"")
                        .arg(replacedFact, plan.memoryFact);
      else
        notifBody = plan.memoryFact;

      NotificationService::instance().needsUserInput(
          isDelete ? tr("Memory deletion")
                   : (!plan.memoryReplaces.isEmpty()
                          ? tr("Memory edit")
                          : tr("Memory proposal")),
          notifBody,
          QString(),
          key,
          m_sessionName);
    }

    announceRequestFinished(requestId, true, summary);

    // A request sitting in "awaiting" is not terminal. Do not run the
    // terminal-dependent pass; the request will be moved to done or
    // rejected when the user resolves the proposal.
    if (autoAccepted)
      failBlockedDependentsAfterTerminal(requestId);

    return;
  }

  if (plan.kind == DispatchPlan::Kind::Defer) {
    m_queue->setDeferred(requestId, true);
    m_queue->setState(requestId, QStringLiteral("inbox"));

    appendActionSummary(
        QStringLiteral("Deferred request %1 until its dependencies "
                       "complete.").arg(requestId));
    return;
  }

  if (plan.kind == DispatchPlan::Kind::Redirect) {
    FileAgent *agent = fileAgentById(plan.agentId);

    if (!agent) {
      const QString reason =
          tr("Write owner %1 is no longer available.").arg(plan.agentId);

      m_queue->setState(requestId, QStringLiteral("failed"));
      m_queue->setRejectReason(requestId, reason);

      announceRequestFinished(requestId, false, reason);
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
      const QString reason = tr("No such worker: %1").arg(plan.agentId);

      m_queue->setState(requestId, QStringLiteral("failed"));
      m_queue->setRejectReason(requestId, reason);

      announceRequestFinished(requestId, false, reason);
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

  if (plan.kind != DispatchPlan::Kind::SpawnAgent) {
    const QString reason =
        tr("Internal error: unknown dispatch kind for request %1.")
            .arg(requestId);

    m_queue->setState(requestId, QStringLiteral("failed"));
    m_queue->setRejectReason(requestId, reason);

    announceRequestFinished(requestId, false, reason);
    return;
  }

  const int cap = Settings::getOverseerFileAgentCap();

  if (m_fileAgents.size() >= cap) {
    FileAgent *fallback = leastLoadedAgent();

    if (!fallback) {
      const QString reason =
          tr("File agent cap reached and no agent is available.");

      m_queue->setState(requestId, QStringLiteral("failed"));
      m_queue->setRejectReason(requestId, reason);

      announceRequestFinished(requestId, false, reason);
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
    const QString reason = tr("Could not spawn a file agent.");

    m_queue->setState(requestId, QStringLiteral("failed"));
    m_queue->setRejectReason(requestId, reason);

    announceRequestFinished(requestId, false, reason);
    return;
  }

  FileAgent *agent = fileAgentById(agentId);

  if (!agent) {
    const QString reason = tr("Spawned agent is not available.");

    m_queue->setState(requestId, QStringLiteral("failed"));

    announceRequestFinished(requestId, false, reason);
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

void OverseerRunner::handleSpawnScopedEdit(const QString &requestId,
                                           const QString &filePath,
                                           const QString &instruction,
                                           const QString &originAgentId,
                                           const QString &originTaskId) {
  if (!m_inferenceService || !m_focusedEditor) {
    const QString reason = tr("No editor available for the target file.");

    m_queue->setState(requestId, QStringLiteral("failed"));
    m_queue->setRejectReason(requestId, reason);

    announceRequestFinished(requestId, false, reason);
    return;
  }

  const QString absolute =
      QDir(m_session->outputPath()).absoluteFilePath(filePath);

  if (m_workstation && m_workstation->isFileLocked(absolute)) {
    const QString reason =
        tr("A scoped edit is already active on %1.")
            .arg(QFileInfo(absolute).fileName());

    m_queue->setState(requestId, QStringLiteral("failed"));
    m_queue->setRejectReason(requestId, reason);

    announceRequestFinished(requestId, false, reason);
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

  emit planGenerationStarted(absolute);

  planner->start(m_focusedEditor, instruction);
}

void OverseerRunner::onPlannerValidated(
    const QString &planId, const QVector<EditCommand> &commands) {
  auto it = m_scopedSessions.find(planId);

  if (it == m_scopedSessions.end())
    return;

  ScopedSession &ctx = it.value();

  if (commands.isEmpty()) {
    const QString reason = tr("The planner returned no edits.");

    m_queue->setState(ctx.requestId, QStringLiteral("failed"));
    m_queue->setRejectReason(ctx.requestId, reason);

    announceRequestFinished(ctx.requestId, false, reason);

    tearDownScopedSession(planId);
    return;
  }

  if (!m_focusedEditor) {
    const QString reason = tr("No editor available.");

    m_queue->setState(ctx.requestId, QStringLiteral("failed"));
    m_queue->setRejectReason(ctx.requestId, reason);

    announceRequestFinished(ctx.requestId, false, reason);

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

            announceRequestFinished(requestId, false, reason);

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

              sessionIt->awaitingReview = true;

              emit planReviewReady(sessionIt->filePath);

              if (m_sessionSettings.effectiveAutoEdits()) {
                autoApplySession(planId);
              } else {
                NotificationService::instance().needsUserInput(
                    tr("Edit plan ready"),
                    QFileInfo(sessionIt->filePath).fileName(),
                    sessionIt->filePath,
                    planId,
                    m_sessionName);
              }

              emit changed();
            } else {
              const QString reason = tr("Some edits did not generate.");

              m_queue->setState(requestId, QStringLiteral("failed"));
              m_queue->setRejectReason(requestId, reason);

              announceRequestFinished(requestId, false, reason);
            }
          });

  if (!session->executePlan(commands)) {
    const QString reason =
        tr("Plan could not be resolved against the document.");

    m_queue->setState(ctx.requestId, QStringLiteral("failed"));
    m_queue->setRejectReason(ctx.requestId, reason);

    announceRequestFinished(ctx.requestId, false, reason);

    tearDownScopedSession(planId);
    return;
  }

  buildEditPlanEvent(ctx);

  session->startAllPendingEdits();
}

void OverseerRunner::buildEditPlanEvent(const ScopedSession &ctx) {
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

void OverseerRunner::autoApplySession(const QString &planId) {
  auto it = m_scopedSessions.find(planId);

  if (it == m_scopedSessions.end())
    return;

  if (!m_sessionSettings.effectiveAutoEdits())
    return;

  const QString requestId = it->requestId;
  const QString filePath = it->filePath;

  if (!it->session || !it->session->applyAcceptedPendingEdits()) {
    const QString reason = tr("Failed to apply edits.");

    m_queue->setState(requestId, QStringLiteral("failed"));
    m_queue->setRejectReason(requestId, reason);

    announceRequestFinished(requestId, false, reason);

    tearDownScopedSession(planId);
    return;
  }

  if (!filePath.isEmpty())
    emit saveWorkstationFileRequested(filePath);

  it->awaitingReview = false;

  m_queue->setState(requestId, QStringLiteral("done"));

  appendActionSummary(
      QStringLiteral("Applied %1 for request %2")
          .arg(QFileInfo(filePath).fileName(), requestId));

  if (m_transcriptStore)
    m_transcriptStore->updatePlanStatus(
        planId, QStringLiteral("applied"), QString());

  emit planApplied(filePath);

  announceRequestFinished(
      requestId, true,
      tr("Applied %1.").arg(QFileInfo(filePath).fileName()), filePath);

  NotificationService::instance().info(
      tr("Applied"),
      tr("%1 has been updated.").arg(QFileInfo(filePath).fileName()),
      filePath);

  tearDownScopedSession(planId);
}

void OverseerRunner::onPlannerFailed(const QString &planId,
                                     const QString &reason) {
  auto it = m_scopedSessions.find(planId);

  if (it == m_scopedSessions.end())
    return;

  const QString requestId = it->requestId;

  m_queue->setState(requestId, QStringLiteral("failed"));
  m_queue->setRejectReason(requestId, reason);

  appendActionSummary(
      QStringLiteral("Plan for %1 failed: %2").arg(requestId, reason));

  emit planFailed(it->filePath);

  announceRequestFinished(requestId, false, reason);

  tearDownScopedSession(planId);
}

void OverseerRunner::tearDownScopedSession(const QString &planId) {
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

  emit changed();
}

void OverseerRunner::refreshFileAgentMemory() {
  if (m_fileAgents.isEmpty())
    return;

  const QString globalRaw = OverseerStorage::readMemory();
  const QString sessionRaw = m_session ? m_session->memory() : QString();

  const QStringList globalFacts = parseFacts(globalRaw);
  const QStringList sessionFacts = parseFacts(sessionRaw);

  for (FileAgent *agent : std::as_const(m_fileAgents)) {
    if (agent)
      agent->setMemoryFacts(globalFacts, sessionFacts);
  }
}

QString OverseerRunner::spawnFileAgent(const QString &domain) {
  const QString id =
      QStringLiteral("agent-fs-%1").arg(m_nextAgentOrdinal++);

  auto *agent = new FileAgent(id, domain, m_inferenceService, &m_tools,
                              sessionLogger(), this);

  if (m_session)
    agent->setOutputFolder(m_session->outputPath());

  agent->setToolCallDepthLimit(m_toolCallDepthLimit);

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

                  announceRequestFinished(requestId, false, result);
                }
              } else {
                m_queue->setState(requestId, QStringLiteral("done"));

                appendActionSummary(
                    QStringLiteral("Completed request %1.")
                        .arg(requestId));

                announceRequestFinished(requestId, true, result);
              }
            }

            m_taskToAgent.remove(taskId);
            syncRoster();

            settleDependentRequests();

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

  connect(agent, &FileAgent::depthLimitReached, this,
          [this](const QString &agentId, int limit) {
            appendActionSummary(
                QStringLiteral("Agent %1 hit its tool call depth limit "
                               "(%2).")
                    .arg(agentId)
                    .arg(limit));

            emit agentDepthLimitReached(agentId, limit);

            emit changed();
          });

  m_fileAgents.insert(id, agent);

  refreshFileAgentMemory();

  syncRoster();

  return id;
}

FileAgent *OverseerRunner::fileAgentById(const QString &id) const {
  return m_fileAgents.value(id, nullptr);
}

void OverseerRunner::syncRoster() {
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

  emit changed();
}

QString OverseerRunner::buildConductorPrompt(
    const ConductorRequest &request) const {
  const QString globalMemoryRaw = OverseerStorage::readMemory();
  const QString sessionMemoryRaw =
      m_session ? m_session->memory() : QString();

  auto renderKeyedFacts = [](const QString &raw) {
    const QStringList facts = parseFacts(raw);

    if (facts.isEmpty())
      return QStringLiteral("(no facts)");

    QString out;

    for (const QString &f : facts) {
      out += QStringLiteral("[%1] %2\n")
                 .arg(factKey(f, QStringLiteral("global")), f);
    }

    while (out.endsWith(QChar('\n')))
      out.chop(1);

    return out;
  };

  auto renderSessionFacts = [](const QString &raw) {
    const QStringList facts = parseFacts(raw);

    if (facts.isEmpty())
      return QStringLiteral("(no facts)");

    QString out;

    for (const QString &f : facts) {
      out += QStringLiteral("[%1] %2\n")
                 .arg(factKey(f, QStringLiteral("session")), f);
    }

    while (out.endsWith(QChar('\n')))
      out.chop(1);

    return out;
  };

  const QString globalMemory = renderKeyedFacts(globalMemoryRaw);
  const QString sessionMemory = renderSessionFacts(sessionMemoryRaw);

  // Pending memory proposals: facts the user hasn't yet accepted or
  // rejected. The conductor needs to see them so it can declare a
  // dependency rather than answer as if the fact exists.
  QString pendingSection;

  {
    QStringList lines;

    for (const MemoryProposal &p : std::as_const(m_proposals)) {
      if (p.status != QStringLiteral("pending"))
        continue;

      const QString requestId = p.requestId.isEmpty()
                                    ? QStringLiteral("?")
                                    : p.requestId.left(8);

      if (p.fact.isEmpty() && !p.replaces.isEmpty()) {
        lines.append(QStringLiteral("- (delete) %1 [request %2]")
                         .arg(p.replacedFact, requestId));
      } else if (!p.replaces.isEmpty()) {
        lines.append(QStringLiteral("- (replace) %1 [request %2]")
                         .arg(p.fact, requestId));
      } else {
        lines.append(QStringLiteral("- %1 [request %2]")
                         .arg(p.fact, requestId));
      }
    }

    pendingSection = lines.isEmpty()
                         ? QStringLiteral("(no pending proposals)")
                         : lines.join(QChar('\n'));
  }

  QString directoryHighlights;

  if (m_session) {
    const QString output = m_session->outputPath();

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

  // Recent requests with their ids and current states. The conductor
  // needs this to cite request ids in "depends_on".
  QString requestHistory;

  {
    QStringList lines;

    const QVector<ConductorRequest> all = m_queue->all();

    const int start = qMax(0, all.size() - 12);

    for (int i = start; i < all.size(); ++i) {
      const ConductorRequest &r = all.at(i);

      QString line = QStringLiteral("%1  [%2]  %3")
                         .arg(r.id.left(8), r.state,
                              r.text.simplified().left(80));

      if (!r.blockedOn.isEmpty()) {
        QStringList blockers;

        for (const QString &b : r.blockedOn)
          blockers.append(b.left(8));

        line += QStringLiteral("  (waiting on %1)")
                    .arg(blockers.join(QStringLiteral(", ")));
      }

      lines.append(line);
    }

    requestHistory = lines.isEmpty() ? QStringLiteral("(no requests)")
                                     : lines.join(QChar('\n'));
  }

  QString depsSection;

  if (m_session) {
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
                   .arg(node.left(8), froms.join(QStringLiteral(", ")));
    }

    if (edges.isEmpty())
      edges = QStringLiteral("(no dependencies recorded)");

    depsSection += QStringLiteral("\nEdges:\n%1").arg(edges);
  }

  const QString workingRoot = m_session ? m_session->outputPath() : QString();

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
      "  {\"action\": \"propose_memory\",\n"
      "   \"fact\": \"...\",\n"
      "   \"rationale\": \"...\",\n"
      "   \"scope\": \"global\" | \"session\"}\n"
      "  {\"action\": \"propose_memory\",\n"
      "   \"fact\": \"<new text>\",\n"
      "   \"rationale\": \"...\",\n"
      "   \"scope\": \"global\" | \"session\",\n"
      "   \"replaces\": \"<key>\"}\n"
      "  {\"action\": \"propose_memory\",\n"
      "   \"fact\": \"\",\n"
      "   \"scope\": \"global\" | \"session\",\n"
      "   \"replaces\": \"<key>\"}\n"
      "  {\"action\": \"batch\",\n"
      "   \"actions\": [\n"
      "     {\"action\": \"...\"},\n"
      "     {\"action\": \"...\"},\n"
      "     ...\n"
      "   ]}\n"
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
      "Any single action (not a batch) may additionally carry a\n"
      "\"depends_on\" array of request ids this request needs completed\n"
      "first.\n"
      "\n"
      "## Declaring dependencies\n"
      "\n"
      "You MUST declare depends_on when the current request depends on\n"
      "the outcome of an earlier request. Specifically:\n"
      "\n"
      "* If the user is asking about something that is currently a\n"
      "  pending memory proposal (see the Pending proposals section),\n"
      "  declare depends_on the request id shown in brackets next to\n"
      "  that proposal. Do not answer from facts that are not yet\n"
      "  accepted.\n"
      "* If the current request modifies a file or fact that a recent\n"
      "  request produced, declare depends_on that request.\n"
      "* If the user's text clearly chains (\"then\", \"after that\",\n"
      "  \"once that's done\", \"and then\"), declare depends_on the\n"
      "  antecedent.\n"
      "\n"
      "A request whose depends_on targets are not yet complete sits\n"
      "blocked until they are. Answering from a fact that is still\n"
      "pending acceptance produces a wrong answer.\n"
      "\n"
      "Example: the user sends \"Remember I prefer British spelling.\"\n"
      "and then immediately \"Tell me what my spelling preference is.\"\n"
      "The first request becomes a pending proposal with a request id.\n"
      "The second request should be:\n"
      "  {\"action\": \"answer\", \"text\": \"...\",\n"
      "   \"depends_on\": [\"<id of the first request>\"]}\n"
      "The answer is produced only after the user accepts the pending\n"
      "proposal, so it can cite the fact correctly.\n"
      "\n"
      "\"domain\" is a short label for a body of related work. It is\n"
      "NOT a file path and NOT a directory. Use a single word such as\n"
      "\"recipes\" or \"hastings\". Do not end it with a slash.\n"
      "\n"
      "Use \"answer\" for pure questions. Use \"reject\" when the\n"
      "request cannot be satisfied.\n"
      "\n"
      "Use the first \"propose_memory\" shape to add a NEW fact. Use\n"
      "the second to REPLACE an existing fact: set \"replaces\" to\n"
      "the bracketed key shown next to the fact in the memory\n"
      "sections below. Cite the key exactly as shown, with no\n"
      "additional suffix. Use the third to DELETE an existing fact:\n"
      "leave \"fact\" empty and set \"replaces\" to the fact's key.\n"
      "\n"
      "Before adding a new fact, check whether the memory sections\n"
      "below already contain a fact that says the same thing. If a\n"
      "fact is already present, do not propose it again. If the\n"
      "user's statement slightly refines an existing fact, use the\n"
      "replace shape rather than adding a second near-duplicate.\n"
      "\n"
      "Use \"batch\" when the user's request is really several\n"
      "independent actions at once. Every sub-action inside a batch\n"
      "must be a full action object of the same shapes listed above,\n"
      "except that a batch cannot itself contain a nested batch. The\n"
      "sub-actions become independent tasks. Use batch when the user\n"
      "asks for multiple unrelated things in one message. Do not use\n"
      "batch when the sub-actions depend on each other; if the\n"
      "sub-actions need ordering, send them as separate requests with\n"
      "depends_on, or answer the user with the sequence instead.\n"
      "\n"
      "The \"fact\" field is one sentence. The \"rationale\" is\n"
      "optional but encouraged. The \"scope\" is \"global\" for a\n"
      "fact that should apply to every session, or \"session\" for a\n"
      "fact that applies only to this session. The \"replaces\" key\n"
      "MUST come from the keyed lists below; do not invent one.\n"
      "\n"
      "Use \"spawn_scoped_edit\" when the request is a structural\n"
      "edit to a specific file that already exists. Use\n"
      "\"route_to_worker\" when an existing file agent already knows\n"
      "the files this request names. Use \"spawn_file_agent\" only\n"
      "when no existing agent has any knowledge of the files this\n"
      "request names.\n"
      "\n"
      "Do NOT use \"spawn_file_agent\" if the Agent capacity section\n"
      "below says you are at capacity. Use \"route_to_worker\" for an\n"
      "existing agent, or \"reject\" with a reason.\n"
      "\n"
      "## Global memory (keyed)\n\n%2\n\n"
      "## Session memory (keyed)\n\n%3\n\n"
      "## Pending proposals (not yet accepted)\n\n%4\n\n"
      "## Recent requests\n\n%5\n\n"
      "## File directory highlights\n\n%6\n\n"
      "## Agent roster\n\n%7\n\n"
      "## Agent capacity\n\n%8\n\n"
      "## Writes in flight\n\n%9\n\n"
      "## Recent conductor actions\n\n%10\n\n"
      "## Dependency state\n\n%11\n\n"
      "## Current request\n\n%12\n")
      .arg(workingRoot, globalMemory, sessionMemory, pendingSection,
           requestHistory, directoryHighlights, agents, capacity, writers,
           actionHistory, depsSection, request.text);

  return prompt;
}

void OverseerRunner::appendActionSummary(const QString &line) {
  if (!m_session)
    return;

  m_actionSummary.append(line);

  while (m_actionSummary.size() > 50)
    m_actionSummary.removeFirst();

  QJsonArray arr;

  for (const QString &entry : std::as_const(m_actionSummary))
    arr.append(entry);

  QFile file(QDir(m_session->folderPath())
                 .filePath(ConductorActionSummaryFilename));

  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate |
                 QIODevice::Text))
    return;

  file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
}

void OverseerRunner::acceptProposal(const QString &key,
                                    const QString &scope) {
  setProposalStatus(key, QStringLiteral("accepted"), scope);
}

void OverseerRunner::rejectProposal(const QString &key) {
  setProposalStatus(key, QStringLiteral("rejected"), QString());
}

QString OverseerRunner::proposalsSidecarPath() const {
  if (!m_session)
    return {};
  return QDir(m_session->folderPath())
      .filePath(QStringLiteral("proposals.json"));
}

void OverseerRunner::loadProposals() {
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
    proposal.replaces = obj.value(QStringLiteral("replaces")).toString();
    proposal.replacedFact =
        obj.value(QStringLiteral("replacedFact")).toString();
    proposal.acceptedScope =
        obj.value(QStringLiteral("acceptedScope")).toString();
    proposal.fallbackNote =
        obj.value(QStringLiteral("fallbackNote")).toString();
    proposal.requestId = obj.value(QStringLiteral("requestId")).toString();

    if (proposal.key.isEmpty())
      continue;

    m_proposals.append(proposal);
  }
}

void OverseerRunner::saveProposals() {
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
    obj.insert(QStringLiteral("replaces"), p.replaces);
    obj.insert(QStringLiteral("replacedFact"), p.replacedFact);
    obj.insert(QStringLiteral("acceptedScope"), p.acceptedScope);
    obj.insert(QStringLiteral("fallbackNote"), p.fallbackNote);
    obj.insert(QStringLiteral("requestId"), p.requestId);
    arr.append(obj);
  }

  QFile file(path);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
    return;

  file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
}

QString OverseerRunner::recordProposal(const QString &requestId,
                                       const QString &fact,
                                       const QString &rationale,
                                       const QString &scope,
                                       const QString &replacesKey) {
  const QString trimmedFact = fact.trimmed();
  const QString trimmedReplaces = normaliseFactKey(replacesKey);

  if (trimmedFact.isEmpty() && trimmedReplaces.isEmpty())
    return {};

  for (const MemoryProposal &p : std::as_const(m_proposals)) {
    if (p.scope != scope)
      continue;

    if (p.replaces != trimmedReplaces)
      continue;

    if (p.fact != trimmedFact)
      continue;

    if (p.status == QStringLiteral("pending") ||
        p.status == QStringLiteral("accepted")) {
      return p.key;
    }
  }

  MemoryProposal proposal;
  proposal.fact = trimmedFact;
  proposal.rationale = rationale;
  proposal.scope = scope;
  proposal.replaces = trimmedReplaces;
  proposal.requestId = requestId;

  proposal.key = QString::number(
      qHash(trimmedFact + QChar('|') + scope + QChar('|') + trimmedReplaces));

  if (!trimmedReplaces.isEmpty()) {
    const QString targetFile =
        (scope == QStringLiteral("session") && m_session)
            ? m_session->memoryPath()
            : OverseerStorage::memoryPath();

    QFile target(targetFile);

    if (target.open(QIODevice::ReadOnly | QIODevice::Text)) {
      QTextStream stream(&target);
      stream.setEncoding(QStringConverter::Utf8);

      const QStringList facts = parseFacts(stream.readAll());

      for (const QString &f : facts) {
        if (factKey(f, scope) == trimmedReplaces) {
          proposal.replacedFact = f;
          break;
        }
      }
    }
  }

  const QString targetFile =
      (scope == QStringLiteral("session") && m_session)
          ? m_session->memoryPath()
          : OverseerStorage::memoryPath();

  if (m_sessionSettings.effectiveAutoMemory()) {
    bool ok = false;
    bool fellBackToInsert = false;

    if (trimmedReplaces.isEmpty()) {
      ok = OverseerStorage::appendFactToMemoryFile(targetFile, trimmedFact);
    } else if (trimmedFact.isEmpty()) {
      ok = OverseerStorage::removeFactFromMemoryFile(
          targetFile, proposal.replacedFact);
    } else {
      // Try the replace. If the old fact is not present, fall back to
      // an insert and note it.
      const QString current = OverseerStorage::readMemory();

      bool present = false;
      Q_UNUSED(current);

      const QString targetPath =
          (scope == QStringLiteral("session") && m_session)
              ? m_session->memoryPath()
              : OverseerStorage::memoryPath();

      QFile target(targetPath);

      if (target.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&target);
        stream.setEncoding(QStringConverter::Utf8);

        const QStringList facts = parseFacts(stream.readAll());

        present = facts.contains(proposal.replacedFact);
      }

      if (present) {
        ok = OverseerStorage::replaceFactInMemoryFile(
            targetFile, proposal.replacedFact, trimmedFact);
      } else {
        ok = OverseerStorage::appendFactToMemoryFile(targetFile, trimmedFact);
        fellBackToInsert = true;
      }
    }

    if (ok) {
      proposal.status = QStringLiteral("accepted");
      proposal.acceptedScope = scope;

      if (fellBackToInsert) {
        proposal.fallbackNote =
            tr("The fact this proposal would have replaced was no longer "
               "present. It was recorded as a new fact instead.");
      }
    } else {
      proposal.status = QStringLiteral("pending");
    }
  } else {
    proposal.status = QStringLiteral("pending");
  }

  m_proposals.append(proposal);

  saveProposals();

  refreshFileAgentMemory();

  return proposal.key;
}

void OverseerRunner::settleProposalRequest(const QString &proposalKey,
                                           bool accepted) {
  for (const MemoryProposal &p : std::as_const(m_proposals)) {
    if (p.key != proposalKey)
      continue;

    if (p.requestId.isEmpty())
      return;

    const ConductorRequest req = m_queue->byId(p.requestId);

    if (req.id.isEmpty())
      return;

    if (accepted) {
      m_queue->setState(p.requestId, QStringLiteral("done"));
      announceRequestFinished(p.requestId, true, req.answer);
      failBlockedDependentsAfterTerminal(p.requestId);
    } else {
      m_queue->setState(p.requestId, QStringLiteral("rejected"));
      announceRequestFinished(p.requestId, false,
                              tr("The proposal was rejected by the user."));
      failBlockedDependentsAfterTerminal(p.requestId);
    }

    drainQueue();

    return;
  }
}
void OverseerRunner::setProposalStatus(const QString &key,
                                       const QString &status,
                                       const QString &acceptedScope) {
  bool changedAny = false;
  bool applyFailed = false;
  QString failureReason;

  for (MemoryProposal &p : m_proposals) {
    if (p.key != key)
      continue;

    if (status == QStringLiteral("accepted")) {
      const QString scope = p.scope;
      const QString targetPath =
          (scope == QStringLiteral("session") && m_session)
              ? m_session->memoryPath()
              : OverseerStorage::memoryPath();

      bool ok = false;
      bool fellBackToInsert = false;

      if (p.replaces.isEmpty()) {
        ok = OverseerStorage::appendFactToMemoryFile(targetPath, p.fact);
      } else if (p.fact.isEmpty()) {
        ok = OverseerStorage::removeFactFromMemoryFile(targetPath,
                                                       p.replacedFact);
      } else {
        // Check whether the old fact is still present.
        bool present = false;

        QFile target(targetPath);

        if (target.open(QIODevice::ReadOnly | QIODevice::Text)) {
          QTextStream stream(&target);
          stream.setEncoding(QStringConverter::Utf8);

          const QStringList facts = parseFacts(stream.readAll());

          present = facts.contains(p.replacedFact);
        }

        if (present) {
          ok = OverseerStorage::replaceFactInMemoryFile(
              targetPath, p.replacedFact, p.fact);
        } else {
          ok = OverseerStorage::appendFactToMemoryFile(targetPath, p.fact);
          fellBackToInsert = true;
        }
      }

      if (!ok) {
        applyFailed = true;
        failureReason = tr("Could not write the fact to memory.");
        break;
      }

      if (fellBackToInsert) {
        p.fallbackNote =
            tr("The fact this proposal would have replaced was no longer "
               "present. It was recorded as a new fact instead.");
      }

      p.acceptedScope = acceptedScope;
    }

    p.status = status;
    changedAny = true;

    break;
  }

  if (!changedAny)
    return;

  if (applyFailed) {
    for (MemoryProposal &p : m_proposals) {
      if (p.key != key)
        continue;

      p.status = QStringLiteral("failed");
      p.rationale = failureReason.isEmpty() ? p.rationale : failureReason;
      break;
    }

    saveProposals();

    if (m_transcriptStore)
      m_transcriptStore->updateProposalStatus(
          key, QStringLiteral("failed"), QString());

    reloadMemoryPanels();

    emit changed();
    return;
  }

  saveProposals();

  if (m_transcriptStore)
    m_transcriptStore->updateProposalStatus(key, status, acceptedScope);

  const QVector<NotificationService::Notification> pending =
      NotificationService::instance().pending();

  for (const NotificationService::Notification &n : pending) {
    if (n.targetCardId == key)
      NotificationService::instance().acknowledge(n.id);
  }

  refreshFileAgentMemory();

  reloadMemoryPanels();

  emit changed();

  // Move the originating request to its terminal state and free any
  // dependents that were blocked on it.
  settleProposalRequest(key,
                        status == QStringLiteral("accepted"));
}

void OverseerRunner::reloadMemoryPanels() {
  emit changed();
}

PayloadLogger *OverseerRunner::sessionLogger() const {
  return m_sessionLogger ? m_sessionLogger : m_payloadLogger;
}

int OverseerRunner::expertiseForInstruction(const QString &instruction,
                                            const QString &agentId) const {
  if (instruction.isEmpty() || agentId.isEmpty())
    return 0;

  FileAgent *agent = fileAgentById(agentId);

  if (!agent)
    return 0;

  int score = 0;

  if (instructionCollidesWithAgentClaims(instruction, agentId))
    score += 1000;

  const QStringList seen = agent->filesSeen();

  if (seen.isEmpty())
    return score;

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

FileAgent *OverseerRunner::bestExpertForInstruction(
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

FileAgent *OverseerRunner::leastLoadedAgent() const {
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


void OverseerRunner::setToolCallDepthLimit(int limit) {
  m_toolCallDepthLimit = qBound(1, limit, 100000);

  for (FileAgent *agent : std::as_const(m_fileAgents)) {
    if (agent)
      agent->setToolCallDepthLimit(m_toolCallDepthLimit);
  }
}

void OverseerRunner::acceptPlanEdit(const QString &planId, int editId) {
  auto it = m_scopedSessions.find(planId);

  if (it == m_scopedSessions.end() || !it->session)
    return;

  it->session->acceptPendingEdit(editId);
}

void OverseerRunner::rejectPlanEdit(const QString &planId, int editId) {
  auto it = m_scopedSessions.find(planId);

  if (it == m_scopedSessions.end() || !it->session)
    return;

  it->session->rejectPendingEdit(editId);
}

void OverseerRunner::applyPlan(const QString &planId) {
  auto it = m_scopedSessions.find(planId);

  if (it == m_scopedSessions.end() || !it->session)
    return;

  const QString requestId = it->requestId;
  const QString filePath = it->filePath;

  if (!it->session->applyAcceptedPendingEdits()) {
    const QString reason = tr("Failed to apply edits.");

    m_queue->setState(requestId, QStringLiteral("failed"));
    m_queue->setRejectReason(requestId, reason);

    announceRequestFinished(requestId, false, reason);

    tearDownScopedSession(planId);
    return;
  }

  if (!filePath.isEmpty())
    emit saveWorkstationFileRequested(filePath);

  it->awaitingReview = false;

  m_queue->setState(requestId, QStringLiteral("done"));

  appendActionSummary(
      QStringLiteral("Applied %1 for request %2")
          .arg(QFileInfo(filePath).fileName(), requestId));

  if (m_transcriptStore)
    m_transcriptStore->updatePlanStatus(
        planId, QStringLiteral("applied"), QString());

  emit planApplied(filePath);

  announceRequestFinished(
      requestId, true,
      tr("Applied %1.").arg(QFileInfo(filePath).fileName()), filePath);

  NotificationService::instance().info(
      tr("Applied"),
      tr("%1 has been updated.").arg(QFileInfo(filePath).fileName()),
      filePath);

  const QVector<NotificationService::Notification> pendingNotifs =
      NotificationService::instance().pending();

  for (const NotificationService::Notification &n : pendingNotifs) {
    if (n.targetCardId == planId)
      NotificationService::instance().acknowledge(n.id);
  }

  tearDownScopedSession(planId);

  failBlockedDependentsAfterTerminal(requestId);
}

void OverseerRunner::cancelPlan(const QString &planId) {
  auto it = m_scopedSessions.find(planId);

  if (it == m_scopedSessions.end())
    return;

  const QString requestId = it->requestId;

  it->awaitingReview = false;

  if (m_transcriptStore)
    m_transcriptStore->updatePlanStatus(
        planId, QStringLiteral("cancelled"), QString());

  m_queue->setState(requestId, QStringLiteral("rejected"));
  m_queue->setRejectReason(requestId, tr("Cancelled by user."));

  announceRequestFinished(requestId, false, tr("Cancelled by user."));

  const QVector<NotificationService::Notification> pendingNotifs =
      NotificationService::instance().pending();

  for (const NotificationService::Notification &n : pendingNotifs) {
    if (n.targetCardId == planId)
      NotificationService::instance().acknowledge(n.id);
  }

  tearDownScopedSession(planId);

  failBlockedDependentsAfterTerminal(requestId);
}