#include "../../include/overseer/MemoryAgent.h"

#include "OverseerSession.h"
#include "OverseerStorage.h"
#include "PayloadLogger.h"

#include "inference/InferenceService.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QTextStream>
#include <QTimer>
#include <QUuid>

namespace {

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

QString factKey(const QString &fact, const QString &scope) {
  return QString::number(qHash(fact + QChar('|') + scope));
}

QString normaliseFactKey(const QString &raw) {
  const QString trimmed = raw.trimmed();

  const int pipeIndex = trimmed.indexOf(QChar('|'));

  if (pipeIndex > 0)
    return trimmed.left(pipeIndex);

  return trimmed;
}

} // namespace

MemoryAgent::MemoryAgent(const QString &id,
                         OverseerSession *session,
                         InferenceService *inferenceService,
                         PayloadLogger *logger,
                         QObject *parent)
    : QObject(parent), m_id(id), m_session(session),
      m_inferenceService(inferenceService), m_logger(logger) {
  if (!m_inferenceService)
    return;

  connect(m_inferenceService, &InferenceService::llmDelta, this,
          &MemoryAgent::onResponseDelta);
  connect(m_inferenceService, &InferenceService::llmFinished, this,
          &MemoryAgent::onResponseFinished);
  connect(m_inferenceService, &InferenceService::llmError, this,
          &MemoryAgent::onResponseError);

  load();
}

MemoryAgent::~MemoryAgent() = default;

void MemoryAgent::logAgent(const QString &tag, const QString &content) {
  if (!m_logger)
    return;

  m_logger->log(PayloadLogger::Subsystem::Agent,
                QStringLiteral("%1/%2").arg(m_id, tag), content);
}

void MemoryAgent::setToolCallDepthLimit(int limit) {
  m_toolCallDepthLimit = qBound(1, limit, 100000);
}

QString MemoryAgent::memoryPathForScope(const QString &scope) const {
  if (scope == QStringLiteral("session") && m_session)
    return m_session->memoryPath();

  return OverseerStorage::memoryPath();
}

QStringList MemoryAgent::factsForScope(const QString &scope) const {
  QFile file(memoryPathForScope(scope));

  if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    return {};

  QTextStream stream(&file);
  stream.setEncoding(QStringConverter::Utf8);

  return parseFacts(stream.readAll());
}

void MemoryAgent::enqueue(const Task &task) {
  m_queue.append(task);

  logAgent(QStringLiteral("ENQUEUE"),
           QStringLiteral("Task: %1\nRequest: %2\nPreDecided: %3\n"
                          "Instruction:\n%4")
               .arg(task.id, task.requestId)
               .arg(task.preDecidedAction.isEmpty()
                        ? QStringLiteral("false")
                        : QStringLiteral("true"))
               .arg(task.instruction));

  emit stateChanged();

  if (!m_busy)
    scheduleDispatch();
}

bool MemoryAgent::cancel(const QString &taskId) {
  if (m_current.id == taskId) {
    if (m_hasActiveToken && !m_activeToken.isNull() && m_inferenceService)
      m_inferenceService->abortChatRequest(m_activeToken);

    m_activeToken = InferenceService::RequestToken();
    m_hasActiveToken = false;
    m_pendingDispatch = false;
    m_activeTaskId.clear();
    m_accumulated.clear();
    m_taskToolCalls.clear();
    m_busy = false;

    emit taskFinished(taskId, false, QStringLiteral("Cancelled by user."));
    emit stateChanged();

    m_current = Task();

    scheduleDispatch();

    return true;
  }

  for (int i = 0; i < m_queue.size(); ++i) {
    if (m_queue.at(i).id != taskId)
      continue;

    m_queue.removeAt(i);
    emit taskFinished(taskId, false, QStringLiteral("Cancelled by user."));
    emit stateChanged();
    return true;
  }

  return false;
}

void MemoryAgent::finishTask(const QString &taskId, bool ok,
                             const QString &resultOrError) {
  if (m_current.id == taskId) {
    finishCurrent(ok, resultOrError);
    return;
  }

  for (int i = 0; i < m_queue.size(); ++i) {
    if (m_queue.at(i).id != taskId)
      continue;

    m_queue.removeAt(i);
    emit taskFinished(taskId, ok, resultOrError);
    emit stateChanged();
    return;
  }
}

ConductorWorker MemoryAgent::rosterEntry() const {
  ConductorWorker w;
  w.id = m_id;
  w.type = QStringLiteral("memory");
  w.domain = QStringLiteral("memory");
  w.state = m_busy ? QStringLiteral("busy") : QStringLiteral("idle");
  w.queueDepth = m_queue.size();
  return w;
}

QString MemoryAgent::summaryForConductor() const {
  int pending = 0;

  for (const Proposal &p : m_proposals) {
    if (p.status == QStringLiteral("pending"))
      ++pending;
  }

  QString out;
  out += QStringLiteral("%1  domain: memory  state: %2  queue: %3  "
                        "pending proposals: %4\n")
             .arg(m_id,
                  m_busy ? QStringLiteral("busy") : QStringLiteral("idle"))
             .arg(m_queue.size())
             .arg(pending);
  return out;
}

void MemoryAgent::scheduleDispatch() {
  QTimer::singleShot(0, this, [this]() {
    if (m_busy)
      return;

    if (m_queue.isEmpty())
      return;

    beginNextTask();
  });
}

bool MemoryAgent::isCurrentToken(
    const InferenceService::RequestToken &token) const {
  if (!m_hasActiveToken)
    return false;

  if (token.isNull())
    return false;

  return token == m_activeToken;
}

void MemoryAgent::beginNextTask() {
  if (m_busy || m_queue.isEmpty())
    return;

  m_current = m_queue.takeFirst();
  m_busy = true;
  m_accumulated.clear();
  m_taskToolCalls.clear();
  m_activeTaskId = m_current.id;

  emit stateChanged();

  // Pre-decided tasks skip the LLM entirely. The conductor already
  // decided what the operation is; there is nothing for a model to
  // reason about.
  if (!m_current.preDecidedAction.isEmpty()) {
    applyPreDecided(m_current.preDecidedAction);
    return;
  }

  if (!m_inferenceService) {
    finishCurrent(false, QStringLiteral("Inference service unavailable."));
    return;
  }

  dispatchTurn();
}

void MemoryAgent::applyPreDecided(const QJsonObject &action) {
  const QString fact = action.value(QStringLiteral("fact")).toString().trimmed();
  const QString rationale =
      action.value(QStringLiteral("rationale")).toString().trimmed();
  const QString scope =
      action.value(QStringLiteral("scope")).toString(QStringLiteral("global"));
  const QString rawReplaces =
      action.value(QStringLiteral("replaces")).toString().trimmed();

  logAgent(QStringLiteral("PREDECIDED_APPLY"),
           QStringLiteral("Task: %1\nFact: %2\nScope: %3\nReplaces: %4")
               .arg(m_current.id, fact, scope, rawReplaces));

  // Normalise the replaces key. Three cases:
  //   1. Empty -> add.
  //   2. Numeric -> a real key. Use as-is.
  //   3. Non-numeric -> the conductor wrote something else. Try to
  //      resolve it as fact text, or as a hint that names the subject.
  //      Fall back to add if nothing matches.
  QString replaces = rawReplaces;
  bool resolvedToAdd = false;

  if (!replaces.isEmpty()) {
    bool isNumeric = false;
    replaces.toLongLong(&isNumeric);

    if (!isNumeric) {
      // Try exact fact text match first.
      bool matched = false;

      for (const QString &f : factsForScope(scope)) {
        if (f.compare(rawReplaces, Qt::CaseInsensitive) == 0) {
          replaces = factKey(f, scope);
          matched = true;
          break;
        }
      }

      // If exact match failed, treat rawReplaces as a subject hint:
      // find a fact whose text contains the hint as a word.
      if (!matched) {
        QRegularExpression hintRe(
            QStringLiteral("\\b%1\\b")
                .arg(QRegularExpression::escape(rawReplaces)),
            QRegularExpression::CaseInsensitiveOption);

        QStringList candidates;

        for (const QString &f : factsForScope(scope)) {
          if (hintRe.match(f).hasMatch())
            candidates.append(f);
        }

        if (candidates.size() == 1) {
          replaces = factKey(candidates.first(), scope);
          matched = true;
        }
      }

      if (!matched) {
        // Nothing to replace. Degrade to add.
        replaces.clear();
        resolvedToAdd = true;

        logAgent(QStringLiteral("REPLACES_UNRESOLVED"),
                 QStringLiteral("Could not resolve \"%1\" to a fact in "
                                "%2 memory. Treating as add.")
                     .arg(rawReplaces, scope));
      }
    }
  }

  // No replaces at all (either originally empty, or degraded to add).
  if (replaces.isEmpty()) {
    if (fact.isEmpty()) {
      finishCurrent(false,
                    QStringLiteral("Pre-decided action has no fact and no "
                                   "resolvable replaces key."));
      return;
    }

    for (const Proposal &p : std::as_const(m_proposals)) {
      if (p.fact == fact && p.scope == scope &&
          (p.status == QStringLiteral("pending") ||
           p.status == QStringLiteral("accepted"))) {
        finishCurrent(true,
                      QStringLiteral("Fact already present; no change."));
        return;
      }
    }

    Proposal proposal;
    proposal.key = QUuid::createUuid().toString(QUuid::WithoutBraces);
    proposal.fact = fact;
    proposal.rationale = rationale;
    proposal.scope = scope;
    proposal.status = QStringLiteral("pending");
    proposal.requestId = m_current.requestId;

    if (resolvedToAdd) {
      proposal.fallbackNote =
          QStringLiteral("The fact this proposal would have replaced "
                         "could not be identified. It will be recorded "
                         "as a new fact instead.");
    }

    m_proposals.append(proposal);
    save();
    emit proposalsChanged();

    finishCurrent(true,
                  QStringLiteral("Proposed a memory fact: %1").arg(fact));
    return;
  }

  // Replaces is a resolvable key now.
  QString replacedFact;

  for (const QString &f : factsForScope(scope)) {
    if (factKey(f, scope) == replaces) {
      replacedFact = f;
      break;
    }
  }

  if (replacedFact.isEmpty()) {
    // The key did not match anything in the file. Fall back to add
    // (for replace) or no-op (for delete).
    if (fact.isEmpty()) {
      finishCurrent(true,
                    QStringLiteral("The fact was already gone; nothing to "
                                   "delete."));
      return;
    }

    for (const Proposal &p : std::as_const(m_proposals)) {
      if (p.fact == fact && p.scope == scope &&
          (p.status == QStringLiteral("pending") ||
           p.status == QStringLiteral("accepted"))) {
        finishCurrent(true,
                      QStringLiteral("Fact already present; no change."));
        return;
      }
    }

    Proposal proposal;
    proposal.key = QUuid::createUuid().toString(QUuid::WithoutBraces);
    proposal.fact = fact;
    proposal.rationale = rationale;
    proposal.scope = scope;
    proposal.status = QStringLiteral("pending");
    proposal.fallbackNote =
        QStringLiteral("The fact this proposal would have replaced was "
                       "no longer present. It will be recorded as a new "
                       "fact instead.");
    proposal.requestId = m_current.requestId;

    m_proposals.append(proposal);
    save();
    emit proposalsChanged();

    finishCurrent(true,
                  QStringLiteral("Proposed a memory fact (the previous "
                                 "one was already gone): %1")
                      .arg(fact));
    return;
  }

  // Real replace or delete with a resolved key.
  Proposal proposal;
  proposal.key = QUuid::createUuid().toString(QUuid::WithoutBraces);
  proposal.fact = fact;
  proposal.rationale = rationale;
  proposal.scope = scope;
  proposal.replaces = replaces;
  proposal.replacedFact = replacedFact;
  proposal.status = QStringLiteral("pending");
  proposal.requestId = m_current.requestId;

  m_proposals.append(proposal);
  save();
  emit proposalsChanged();

  if (fact.isEmpty()) {
    finishCurrent(true, QStringLiteral("Proposed deleting \"%1\".")
                            .arg(replacedFact));
  } else {
    finishCurrent(true, QStringLiteral("Proposed replacing \"%1\" with "
                                       "\"%2\".")
                            .arg(replacedFact, fact));
  }
}
void MemoryAgent::dispatchTurn() {
  if (m_current.id.isEmpty() || !m_inferenceService)
    return;

  if (m_toolCallCount >= m_toolCallDepthLimit) {
    const QString reason =
        QStringLiteral("Memory agent exceeded its tool call depth "
                       "limit (%1). Stopping.")
            .arg(m_toolCallDepthLimit);

    logAgent(QStringLiteral("DEPTH_LIMIT"), reason);

    emit depthLimitReached(m_id, m_toolCallDepthLimit);

    finishCurrent(false, reason);
    return;
  }

  ++m_toolCallCount;

  m_activeToken = InferenceService::RequestToken();
  m_hasActiveToken = false;
  m_pendingDispatch = true;
  m_accumulated.clear();

  const QString prompt = buildPrompt(m_current);

  QJsonArray messages;
  messages.append(QJsonObject{
      {QStringLiteral("role"), QStringLiteral("system")},
      {QStringLiteral("content"), prompt}});

  logAgent(QStringLiteral("REQUEST"),
           QStringLiteral("Task: %1\nTurn: %2\n\nPrompt:\n%3")
               .arg(m_current.id)
               .arg(m_toolCallCount)
               .arg(prompt));

  const InferenceService::RequestToken token =
      m_inferenceService->sendChatRequest(
          messages, QString(), 0.2, 60000, QString(), QJsonObject(),
          QJsonArray(), m_id);

  m_activeToken = token;
  m_hasActiveToken = !token.isNull();
  m_pendingDispatch = false;

  logAgent(QStringLiteral("TOKEN"),
           QStringLiteral("Task: %1\nToken: %2")
               .arg(m_current.id, token.toString()));
}

void MemoryAgent::onResponseDelta(
    const InferenceService::RequestToken &token, const QString &text) {
  if (m_activeTaskId.isEmpty() || m_current.id.isEmpty())
    return;

  if (m_hasActiveToken) {
    if (token != m_activeToken)
      return;
  } else if (m_pendingDispatch) {
    m_activeToken = token;
    m_hasActiveToken = !token.isNull();
  } else {
    return;
  }

  m_accumulated += text;
}

void MemoryAgent::onResponseFinished(
    const InferenceService::RequestToken &token) {
  if (m_activeTaskId.isEmpty())
    return;

  if (m_hasActiveToken) {
    if (token != m_activeToken)
      return;
  } else if (m_pendingDispatch) {
    m_activeToken = token;
    m_hasActiveToken = !token.isNull();
  } else {
    return;
  }

  if (m_current.id.isEmpty()) {
    m_activeToken = InferenceService::RequestToken();
    m_hasActiveToken = false;
    m_pendingDispatch = false;
    m_activeTaskId.clear();
    return;
  }

  m_activeToken = InferenceService::RequestToken();
  m_hasActiveToken = false;
  m_pendingDispatch = false;

  QString response = m_accumulated.trimmed();
  m_accumulated.clear();

  static const QRegularExpression fenceStart(
      QStringLiteral("^\\s*```[a-zA-Z0-9_-]*\\s*\\n?"));
  static const QRegularExpression fenceEnd(
      QStringLiteral("\\n?\\s*```\\s*$"));

  response.remove(fenceStart);
  response.remove(fenceEnd);
  response = response.trimmed();

  logAgent(QStringLiteral("RESPONSE"),
           QStringLiteral("Task: %1\n\nResponse:\n%2")
               .arg(m_current.id, response));

  QJsonParseError parseError;
  const QJsonDocument doc =
      QJsonDocument::fromJson(response.toUtf8(), &parseError);

  if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
    logAgent(QStringLiteral("PARSE_ERROR"),
             QStringLiteral("Task: %1\nError: %2\nRaw:\n%3")
                 .arg(m_current.id, parseError.errorString(), response));

    int parseErrors = 0;

    for (const QJsonObject &call : std::as_const(m_taskToolCalls)) {
      if (call.value(QStringLiteral("tool")).toString() ==
          QStringLiteral("__parse_error__"))
        ++parseErrors;
    }

    if (parseErrors < kMaxParseRetries) {
      QJsonObject record;
      record.insert(QStringLiteral("tool"),
                    QStringLiteral("__parse_error__"));
      record.insert(QStringLiteral("args"), QJsonObject());
      record.insert(QStringLiteral("ok"), false);
      record.insert(QStringLiteral("result"),
                    QStringLiteral("Your previous response was not a "
                                   "single JSON object. Retry. Raw:\n%1")
                        .arg(response));
      m_taskToolCalls.append(record);

      dispatchTurn();
      return;
    }

    finishCurrent(false,
                  QStringLiteral("Memory agent produced invalid JSON: %1")
                      .arg(parseError.errorString()));
    return;
  }

  applyAction(doc.object());
}

void MemoryAgent::applyAction(const QJsonObject &action) {
  const QString kind = action.value(QStringLiteral("action")).toString();

  if (kind == QStringLiteral("done")) {
    finishCurrent(true, action.value(QStringLiteral("summary")).toString());
    return;
  }

  if (kind == QStringLiteral("fail")) {
    finishCurrent(false, action.value(QStringLiteral("reason")).toString());
    return;
  }

  if (kind == QStringLiteral("list_facts")) {
    const QString scope =
        action.value(QStringLiteral("scope")).toString(QStringLiteral("global"));

    const QStringList facts = factsForScope(scope);

    QJsonObject record;
    record.insert(QStringLiteral("tool"),
                  QStringLiteral("list_facts"));
    record.insert(QStringLiteral("args"), action);
    record.insert(QStringLiteral("ok"), true);
    record.insert(QStringLiteral("result"),
                  facts.isEmpty() ? QStringLiteral("(no facts)")
                                  : facts.join(QChar('\n')));
    m_taskToolCalls.append(record);

    dispatchTurn();
    return;
  }

  if (kind == QStringLiteral("add_fact")) {
    const QString fact = action.value(QStringLiteral("fact")).toString().trimmed();
    const QString rationale =
        action.value(QStringLiteral("rationale")).toString().trimmed();
    const QString scope =
        action.value(QStringLiteral("scope")).toString(QStringLiteral("global"));

    if (fact.isEmpty()) {
      finishCurrent(false, QStringLiteral("add_fact with empty fact."));
      return;
    }

    for (const Proposal &p : std::as_const(m_proposals)) {
      if (p.fact == fact && p.scope == scope &&
          (p.status == QStringLiteral("pending") ||
           p.status == QStringLiteral("accepted"))) {
        finishCurrent(true,
                      QStringLiteral("Fact already present; no change."));
        return;
      }
    }

    Proposal proposal;
    proposal.key = QUuid::createUuid().toString(QUuid::WithoutBraces);
    proposal.fact = fact;
    proposal.rationale = rationale;
    proposal.scope = scope;
    proposal.status = QStringLiteral("pending");
    proposal.requestId = m_current.requestId;

    m_proposals.append(proposal);
    save();
    emit proposalsChanged();

    QJsonObject record;
    record.insert(QStringLiteral("tool"), QStringLiteral("add_fact"));
    record.insert(QStringLiteral("args"), action);
    record.insert(QStringLiteral("ok"), true);
    record.insert(QStringLiteral("result"),
                  QStringLiteral("Proposal recorded: %1").arg(proposal.key));
    m_taskToolCalls.append(record);

    finishCurrent(true,
                  QStringLiteral("Proposed a memory fact: %1").arg(fact));
    return;
  }

  if (kind == QStringLiteral("replace_fact")) {
    const QString replaces =
        normaliseFactKey(action.value(QStringLiteral("replaces")).toString());
    const QString fact = action.value(QStringLiteral("fact")).toString().trimmed();
    const QString rationale =
        action.value(QStringLiteral("rationale")).toString().trimmed();
    const QString scope =
        action.value(QStringLiteral("scope")).toString(QStringLiteral("global"));

    if (replaces.isEmpty() || fact.isEmpty()) {
      finishCurrent(false,
                    QStringLiteral("replace_fact requires 'replaces' and "
                                   "'fact'."));
      return;
    }

    QString replacedFact;

    for (const QString &f : factsForScope(scope)) {
      if (factKey(f, scope) == replaces) {
        replacedFact = f;
        break;
      }
    }

    Proposal proposal;
    proposal.key = QUuid::createUuid().toString(QUuid::WithoutBraces);
    proposal.fact = fact;
    proposal.rationale = rationale;
    proposal.scope = scope;
    proposal.replaces = replaces;
    proposal.replacedFact = replacedFact;
    proposal.status = QStringLiteral("pending");
    proposal.requestId = m_current.requestId;

    if (replacedFact.isEmpty()) {
      finishCurrent(false,
                    QStringLiteral("The fact to replace (%1) is not "
                                   "present in %2 memory.")
                        .arg(replaces.left(8), scope));
      return;
    }

    m_proposals.append(proposal);
    save();
    emit proposalsChanged();

    finishCurrent(true, QStringLiteral("Proposed replacing \"%1\" with "
                                       "\"%2\".").arg(replacedFact, fact));
    return;
  }

  if (kind == QStringLiteral("delete_fact")) {
    const QString replaces =
        normaliseFactKey(action.value(QStringLiteral("replaces")).toString());
    const QString scope =
        action.value(QStringLiteral("scope")).toString(QStringLiteral("global"));

    if (replaces.isEmpty()) {
      finishCurrent(false, QStringLiteral("delete_fact requires 'replaces'."));
      return;
    }

    QString replacedFact;

    for (const QString &f : factsForScope(scope)) {
      if (factKey(f, scope) == replaces) {
        replacedFact = f;
        break;
      }
    }

    if (replacedFact.isEmpty()) {
      finishCurrent(true,
                    QStringLiteral("The fact was already gone; nothing to "
                                   "delete."));
      return;
    }

    Proposal proposal;
    proposal.key = QUuid::createUuid().toString(QUuid::WithoutBraces);
    proposal.scope = scope;
    proposal.replaces = replaces;
    proposal.replacedFact = replacedFact;
    proposal.status = QStringLiteral("pending");
    proposal.requestId = m_current.requestId;

    m_proposals.append(proposal);
    save();
    emit proposalsChanged();

    finishCurrent(true, QStringLiteral("Proposed deleting \"%1\".")
                            .arg(replacedFact));
    return;
  }

  finishCurrent(false,
                QStringLiteral("Unknown memory action: %1").arg(kind));
}

void MemoryAgent::onResponseError(
    const InferenceService::RequestToken &token, const QString &error) {
  if (m_activeTaskId.isEmpty())
    return;

  if (m_hasActiveToken) {
    if (token != m_activeToken)
      return;
  } else if (m_pendingDispatch) {
    m_activeToken = token;
    m_hasActiveToken = !token.isNull();
  } else {
    return;
  }

  m_activeToken = InferenceService::RequestToken();
  m_hasActiveToken = false;
  m_pendingDispatch = false;

  logAgent(QStringLiteral("ERROR"),
           QStringLiteral("Task: %1\n%2").arg(m_current.id, error));

  finishCurrent(false, error);
}

void MemoryAgent::finishCurrent(bool ok, const QString &resultOrError) {
  m_busy = false;

  const QString taskId = m_current.id;

  logAgent(QStringLiteral("TASK_FINISHED"),
           QStringLiteral("Task: %1\nOK: %2\nResult:\n%3")
               .arg(taskId,
                    ok ? QStringLiteral("true") : QStringLiteral("false"))
               .arg(resultOrError));

  emit taskFinished(taskId, ok, resultOrError);
  emit stateChanged();

  m_activeToken = InferenceService::RequestToken();
  m_hasActiveToken = false;
  m_pendingDispatch = false;
  m_activeTaskId.clear();
  m_accumulated.clear();
  m_taskToolCalls.clear();
  m_current = Task();

  if (!m_queue.isEmpty())
    scheduleDispatch();
}

QString MemoryAgent::buildPrompt(const Task &task) const {
  QString prompt;

  prompt += QStringLiteral(
      "You are the memory agent for an Overseer session. You are the\n"
      "only thing that touches the user's memory files. You reason about\n"
      "what the user's instruction means and choose one action.\n"
      "\n"
      "You do not write directly. You propose. Each proposal is shown\n"
      "to the user, who accepts or rejects it. Only on accept is the\n"
      "fact written.\n"
      "\n"
      "Respond with exactly one JSON object. No markdown fences. No\n"
      "explanatory text.\n"
      "\n"
      "The object has exactly one of these shapes:\n"
      "\n"
      "  {\"action\": \"list_facts\", \"scope\": \"global\" | \"session\"}\n"
      "\n"
      "  {\"action\": \"add_fact\",\n"
      "   \"fact\": \"...\",\n"
      "   \"rationale\": \"...\",\n"
      "   \"scope\": \"global\" | \"session\"}\n"
      "\n"
      "  {\"action\": \"replace_fact\",\n"
      "   \"replaces\": \"<key>\",\n"
      "   \"fact\": \"<new text>\",\n"
      "   \"rationale\": \"...\",\n"
      "   \"scope\": \"global\" | \"session\"}\n"
      "\n"
      "  {\"action\": \"delete_fact\",\n"
      "   \"replaces\": \"<key>\",\n"
      "   \"scope\": \"global\" | \"session\"}\n"
      "\n"
      "  {\"action\": \"done\", \"summary\": \"...\"}\n"
      "\n"
      "  {\"action\": \"fail\", \"reason\": \"...\"}\n"
      "\n"
      "\"scope\" is \"global\" for a fact that applies to every session,\n"
      "\"session\" for a fact that applies only to this session. Default\n"
      "to \"global\" for facts about the user themselves.\n"
      "\n"
      "\"fact\" is one sentence. \"rationale\" is optional but\n"
      "encouraged: state briefly why this fact should be remembered.\n"
      "\n"
      "The \"replaces\" key MUST come from the memory sections below.\n"
      "Do not invent one. Cite the bracketed key exactly as shown, with\n"
      "no additional suffix.\n");

  prompt += QStringLiteral("\n## Global memory\n\n");

  {
    const QStringList facts =
        factsForScope(QStringLiteral("global"));

    if (facts.isEmpty()) {
      prompt += QStringLiteral("(no facts)\n");
    } else {
      for (const QString &f : facts) {
        prompt += QStringLiteral("[%1] %2\n")
                      .arg(factKey(f, QStringLiteral("global")), f);
      }
    }
  }

  prompt += QStringLiteral("\n## Session memory\n\n");

  {
    const QStringList facts =
        factsForScope(QStringLiteral("session"));

    if (facts.isEmpty()) {
      prompt += QStringLiteral("(no facts)\n");
    } else {
      for (const QString &f : facts) {
        prompt += QStringLiteral("[%1] %2\n")
                      .arg(factKey(f, QStringLiteral("session")), f);
      }
    }
  }

  prompt += QStringLiteral("\n## Current task\n\n%1\n")
                .arg(task.instruction);

  return prompt;
}

void MemoryAgent::load() {
  m_proposals.clear();

  const QString path = proposalsPath();

  if (path.isEmpty() || !QFileInfo::exists(path))
    return;

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    return;

  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
  if (!doc.isArray())
    return;

  for (const QJsonValue &v : doc.array()) {
    if (!v.isObject())
      continue;

    const QJsonObject obj = v.toObject();

    Proposal p;
    p.key = obj.value(QStringLiteral("key")).toString();
    p.fact = obj.value(QStringLiteral("fact")).toString();
    p.rationale = obj.value(QStringLiteral("rationale")).toString();
    p.status = obj.value(QStringLiteral("status"))
                   .toString(QStringLiteral("pending"));
    p.scope = obj.value(QStringLiteral("scope"))
                  .toString(QStringLiteral("global"));
    p.replaces = obj.value(QStringLiteral("replaces")).toString();
    p.replacedFact = obj.value(QStringLiteral("replacedFact")).toString();
    p.acceptedScope = obj.value(QStringLiteral("acceptedScope")).toString();
    p.fallbackNote = obj.value(QStringLiteral("fallbackNote")).toString();
    p.requestId = obj.value(QStringLiteral("requestId")).toString();

    if (p.key.isEmpty())
      continue;

    m_proposals.append(p);
  }
}

void MemoryAgent::save() const {
  const QString path = proposalsPath();

  if (path.isEmpty())
    return;

  QJsonArray arr;

  for (const Proposal &p : m_proposals) {
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
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate |
                 QIODevice::Text))
    return;

  file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
}

QString MemoryAgent::proposalsPath() const {
  if (!m_session)
    return {};

  return QDir(m_session->folderPath())
      .filePath(QStringLiteral("proposals.json"));
}

bool MemoryAgent::applyFactAdd(const QString &scope, const QString &fact,
                               QString *error) {
  Q_UNUSED(error);

  return OverseerStorage::appendFactToMemoryFile(memoryPathForScope(scope),
                                                 fact);
}

bool MemoryAgent::applyFactReplace(const QString &scope,
                                   const QString &replacedFact,
                                   const QString &newFact,
                                   QString *fallbackNote, QString *error) {
  const QString path = memoryPathForScope(scope);

  const QStringList facts = factsForScope(scope);

  if (facts.contains(replacedFact)) {
    const bool ok =
        OverseerStorage::replaceFactInMemoryFile(path, replacedFact, newFact);
    if (!ok && error)
      *error = QStringLiteral("replaceFactInMemoryFile failed.");
    return ok;
  }

  const bool ok = OverseerStorage::appendFactToMemoryFile(path, newFact);

  if (ok && fallbackNote) {
    *fallbackNote =
        QStringLiteral("The fact this proposal would have replaced was "
                       "no longer present. It was recorded as a new "
                       "fact instead.");
  }

  if (!ok && error)
    *error = QStringLiteral("appendFactToMemoryFile failed.");

  return ok;
}

bool MemoryAgent::applyFactDelete(const QString &scope,
                                  const QString &replacedFact,
                                  QString *error) {
  Q_UNUSED(error);

  return OverseerStorage::removeFactFromMemoryFile(
      memoryPathForScope(scope), replacedFact);
}

bool MemoryAgent::acceptProposal(const QString &key,
                                 const QString &scope) {
  for (Proposal &p : m_proposals) {
    if (p.key != key)
      continue;

    const QString targetScope =
        scope.isEmpty() ? p.scope : scope;

    QString error;
    QString fallbackNote;
    bool ok = false;

    if (p.replaces.isEmpty()) {
      ok = applyFactAdd(targetScope, p.fact, &error);
    } else if (p.fact.isEmpty()) {
      ok = applyFactDelete(targetScope, p.replacedFact, &error);
    } else {
      ok = applyFactReplace(targetScope, p.replacedFact, p.fact,
                            &fallbackNote, &error);
    }

    if (!ok) {
      p.status = QStringLiteral("failed");
      p.rationale = error.isEmpty() ? p.rationale : error;
      save();
      emit proposalsChanged();
      return false;
    }

    if (!fallbackNote.isEmpty())
      p.fallbackNote = fallbackNote;

    p.status = QStringLiteral("accepted");
    p.acceptedScope = targetScope;

    save();
    emit proposalsChanged();
    return true;
  }

  return false;
}

bool MemoryAgent::rejectProposal(const QString &key) {
  for (Proposal &p : m_proposals) {
    if (p.key != key)
      continue;

    p.status = QStringLiteral("rejected");

    save();
    emit proposalsChanged();
    return true;
  }

  return false;
}