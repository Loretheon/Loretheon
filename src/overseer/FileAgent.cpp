#include "../../include/overseer/FileAgent.h"

#include "../../include/agent/ToolRegistry.h"
#include "PayloadLogger.h"

#include "inference/InferenceService.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QTimer>
#include <QUuid>

FileAgent::FileAgent(const QString &id, const QString &domain,
                     InferenceService *inferenceService,
                     ToolRegistry *tools,
                     PayloadLogger *logger,
                     QObject *parent)
    : QObject(parent), m_id(id), m_domain(domain),
      m_inferenceService(inferenceService), m_tools(tools), m_logger(logger) {
  if (!m_inferenceService)
    return;

  connect(m_inferenceService, &InferenceService::llmDelta, this,
          &FileAgent::onResponseDelta);

  connect(m_inferenceService, &InferenceService::llmFinished, this,
          &FileAgent::onResponseFinished);

  connect(m_inferenceService, &InferenceService::llmError, this,
          &FileAgent::onResponseError);
}

FileAgent::~FileAgent() {
  releaseTaskClaims();
}

void FileAgent::logAgent(const QString &tag, const QString &content) {
  if (!m_logger)
    return;

  m_logger->log(PayloadLogger::Subsystem::Agent,
                QStringLiteral("%1/%2").arg(m_id, tag), content);
}

void FileAgent::enqueue(const Task &task) {
  m_queue.append(task);

  logAgent(QStringLiteral("ENQUEUE"),
           QStringLiteral("Task: %1\nRequest: %2\nInstruction:\n%3")
               .arg(task.id, task.requestId, task.instruction));

  emit stateChanged();

  if (!m_busy)
    scheduleDispatch();
}

bool FileAgent::cancel(const QString &taskId) {
  if (m_current.id == taskId) {
    if (m_hasActiveToken && !m_activeToken.isNull() && m_inferenceService)
      m_inferenceService->abortChatRequest(m_activeToken);

    releaseTaskClaims();

    m_activeToken = InferenceService::RequestToken();
    m_hasActiveToken = false;
    m_pendingDispatch = false;
    m_activeTaskId.clear();
    m_accumulated.clear();
    m_taskToolCalls.clear();
    m_busy = false;

    logAgent(QStringLiteral("CANCEL"),
             QStringLiteral("Task: %1 (in flight)").arg(taskId));

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

    logAgent(QStringLiteral("CANCEL"),
             QStringLiteral("Task: %1 (queued)").arg(taskId));

    emit taskFinished(taskId, false, QStringLiteral("Cancelled by user."));
    emit stateChanged();
    return true;
  }

  return false;
}

void FileAgent::finishTask(const QString &taskId, bool ok,
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

ConductorWorker FileAgent::rosterEntry() const {
  ConductorWorker w;
  w.id = m_id;
  w.type = QStringLiteral("file");
  w.domain = m_domain;
  w.state = m_busy ? QStringLiteral("busy") : QStringLiteral("idle");
  w.queueDepth = m_queue.size();
  return w;
}

QString FileAgent::summaryForConductor() const {
  QString out;

  out += QStringLiteral("%1  domain: %2  state: %3  queue: %4\n")
             .arg(m_id, m_domain,
                  m_busy ? QStringLiteral("busy") : QStringLiteral("idle"))
             .arg(m_queue.size());

  if (!m_filesSeen.isEmpty()) {
    out += QStringLiteral("  files seen: %1\n")
               .arg(m_filesSeen.mid(qMax(0, m_filesSeen.size() - 8))
                        .join(QStringLiteral(", ")));
  }

  return out;
}

void FileAgent::setHistory(const QStringList &history) {
  m_history = history;

  while (m_history.size() > kMaxHistory)
    m_history.removeFirst();
}

void FileAgent::setFilesSeen(const QStringList &files) { m_filesSeen = files; }

void FileAgent::scheduleDispatch() {
  QTimer::singleShot(0, this, [this]() {
    if (m_busy)
      return;

    if (m_queue.isEmpty())
      return;

    beginNextTask();
  });
}

bool FileAgent::isCurrentToken(
    const InferenceService::RequestToken &token) const {
  if (!m_hasActiveToken)
    return false;

  if (token.isNull())
    return false;

  return token == m_activeToken;
}

void FileAgent::releaseTaskClaims() {
  for (const QString &path : std::as_const(m_taskWriteClaims)) {
    emit fileWriteReleased(m_current.id, path);
  }

  m_taskWriteClaims.clear();
}

void FileAgent::beginNextTask() {
  if (m_busy)
    return;

  if (m_queue.isEmpty())
    return;

  if (!m_inferenceService)
    return;

  m_current = m_queue.takeFirst();
  m_busy = true;
  m_accumulated.clear();
  m_taskToolCalls.clear();
  m_taskWriteClaims.clear();
  m_activeTaskId = m_current.id;

  emit stateChanged();

  dispatchTurn();
}

void FileAgent::dispatchTurn() {
  if (m_current.id.isEmpty())
    return;

  if (!m_inferenceService)
    return;

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
           QStringLiteral("Task: %1\nTurn tool calls: %2\n\nPrompt:\n%3")
               .arg(m_current.id)
               .arg(m_taskToolCalls.size())
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

void FileAgent::onResponseDelta(
    const InferenceService::RequestToken &token, const QString &text) {
  if (m_activeTaskId.isEmpty())
    return;

  if (m_current.id.isEmpty())
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

void FileAgent::onResponseFinished(
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
             QStringLiteral("Task: %1\nError: %2\nRaw response:\n%3")
                 .arg(m_current.id, parseError.errorString(), response));

    // Count how many parse errors this task has already recovered
    // from. A parse error is recoverable: record the offending reply
    // in the transcript and give the model one correction turn.
    int parseErrors = 0;

    for (const QJsonObject &call : std::as_const(m_taskToolCalls)) {
      if (call.value(QStringLiteral("tool")).toString() ==
          QStringLiteral("__parse_error__")) {
        ++parseErrors;
      }
    }

    if (parseErrors < kMaxParseRetries) {
      QJsonObject record;
      record.insert(QStringLiteral("tool"),
                    QStringLiteral("__parse_error__"));
      record.insert(QStringLiteral("args"), QJsonObject());
      record.insert(QStringLiteral("ok"), false);
      record.insert(QStringLiteral("result"),
                    QStringLiteral("Your previous response was not a "
                                   "single JSON object. It must be exactly "
                                   "one JSON object and nothing else. "
                                   "Previous raw response:\n%1")
                        .arg(response));
      m_taskToolCalls.append(record);

      logAgent(QStringLiteral("PARSE_RETRY"),
               QStringLiteral("Task: %1\nRetrying after parse error.")
                   .arg(m_current.id));

      dispatchTurn();
      return;
    }

    const QString reason =
        response.isEmpty()
            ? QStringLiteral("The model returned an empty response.")
            : QStringLiteral("Agent produced invalid JSON: %1")
                  .arg(parseError.errorString());

    finishCurrent(false, reason);
    return;
  }

  const QJsonObject obj = doc.object();
  const QString action = obj.value(QStringLiteral("action")).toString();

  if (action == QStringLiteral("call_tool")) {
    applyToolCall(obj);
    return;
  }

  if (action == QStringLiteral("delegate_scoped_edit")) {
    DelegateToScopedEdit delegate;
    delegate.filePath = obj.value(QStringLiteral("file")).toString();
    delegate.instruction =
        obj.value(QStringLiteral("instruction")).toString();
    delegate.requestId = m_current.requestId;
    delegate.taskId = m_current.id;

    logAgent(QStringLiteral("DELEGATE"),
             QStringLiteral("Task: %1\nFile: %2\nInstruction:\n%3")
                 .arg(delegate.taskId, delegate.filePath,
                      delegate.instruction));

    emit delegateToScopedEdit(delegate);

    return;
  }

  if (action == QStringLiteral("done")) {
    const QString summary = obj.value(QStringLiteral("summary")).toString();

    logAgent(QStringLiteral("DONE"),
             QStringLiteral("Task: %1\nSummary:\n%2")
                 .arg(m_current.id, summary));

    finishCurrent(true, summary);
    return;
  }

  if (action == QStringLiteral("fail")) {
    const QString reason = obj.value(QStringLiteral("reason")).toString();

    logAgent(QStringLiteral("FAIL"),
             QStringLiteral("Task: %1\nReason:\n%2")
                 .arg(m_current.id, reason));

    finishCurrent(false, reason);
    return;
  }

  logAgent(QStringLiteral("UNKNOWN_ACTION"),
           QStringLiteral("Task: %1\nAction: %2")
               .arg(m_current.id, action));

  finishCurrent(false,
                QStringLiteral("Unknown agent action: %1").arg(action));
}

void FileAgent::onResponseError(
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
           QStringLiteral("Task: %1\nError:\n%2")
               .arg(m_current.id, error));

  finishCurrent(false, error);
}

void FileAgent::applyToolCall(const QJsonObject &toolCall) {
  const QString toolName = toolCall.value(QStringLiteral("tool")).toString();
  const QJsonObject args = toolCall.value(QStringLiteral("args")).toObject();

  if (toolName.isEmpty() || !m_tools) {
    finishCurrent(false, QStringLiteral("Invalid tool call."));
    return;
  }

  Tool::Context context;
  context.outputFolder = m_outputFolder;

  logAgent(QStringLiteral("TOOL_CALL"),
           QStringLiteral("Task: %1\nTool: %2\nArgs:\n%3")
               .arg(m_current.id, toolName,
                    QString::fromUtf8(
                        QJsonDocument(args).toJson(QJsonDocument::Indented))));

  QString claimedPath;

  if (toolName == QStringLiteral("write_file")) {
    claimedPath = args.value(QStringLiteral("path")).toString().trimmed();

    if (!claimedPath.isEmpty()) {
      if (!m_taskWriteClaims.contains(claimedPath))
        m_taskWriteClaims.append(claimedPath);

      emit fileWriteClaimed(m_current.id, claimedPath);

      logAgent(QStringLiteral("WRITE_CLAIM"),
               QStringLiteral("Task: %1\nPath: %2")
                   .arg(m_current.id, claimedPath));
    }
  }

  const Tool::Result result =
      m_tools->execute(toolName, args, context);

  logAgent(QStringLiteral("TOOL_RESULT"),
           QStringLiteral("Task: %1\nTool: %2\nOK: %3\nOutput:\n%4")
               .arg(m_current.id, toolName,
                    result.ok ? QStringLiteral("true")
                              : QStringLiteral("false"))
               .arg(result.ok ? result.output : result.error));

  if (!result.ok) {
    if (!claimedPath.isEmpty()) {
      emit fileWriteReleased(m_current.id, claimedPath);
      m_taskWriteClaims.removeAll(claimedPath);

      logAgent(QStringLiteral("WRITE_RELEASE"),
               QStringLiteral("Task: %1\nPath: %2 (tool failed)")
                   .arg(m_current.id, claimedPath));
    }

    QJsonObject record;
    record.insert(QStringLiteral("tool"), toolName);
    record.insert(QStringLiteral("args"), args);
    record.insert(QStringLiteral("ok"), false);
    record.insert(QStringLiteral("result"), result.error);
    m_taskToolCalls.append(record);

    appendHistory(QStringLiteral("Tool %1 failed: %2")
                      .arg(toolName, result.error));

    finishCurrent(false, result.error);
    return;
  }

  if (toolName == QStringLiteral("read_file") ||
      toolName == QStringLiteral("write_file")) {
    const QString path = args.value(QStringLiteral("path")).toString();

    if (!path.isEmpty() && !m_filesSeen.contains(path)) {
      m_filesSeen.append(path);

      while (m_filesSeen.size() > 50)
        m_filesSeen.removeFirst();
    }
  }

  QJsonObject record;
  record.insert(QStringLiteral("tool"), toolName);
  record.insert(QStringLiteral("args"), args);
  record.insert(QStringLiteral("ok"), true);
  record.insert(QStringLiteral("result"), result.output);
  m_taskToolCalls.append(record);

  appendHistory(QStringLiteral("%1 %2").arg(toolName, result.output.left(120)));

  dispatchTurn();
}

void FileAgent::finishCurrent(bool ok, const QString &resultOrError) {
  m_busy = false;

  const QString taskId = m_current.id;
  const QString requestId = m_current.requestId;

  Q_UNUSED(requestId);

  logAgent(QStringLiteral("TASK_FINISHED"),
           QStringLiteral("Task: %1\nOK: %2\nTurns: %3\nResult:\n%4")
               .arg(taskId,
                    ok ? QStringLiteral("true") : QStringLiteral("false"))
               .arg(m_taskToolCalls.size())
               .arg(resultOrError));

  releaseTaskClaims();

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

void FileAgent::appendHistory(const QString &line) {
  m_history.append(line);

  while (m_history.size() > kMaxHistory)
    m_history.removeFirst();
}

QString FileAgent::buildPrompt(const Task &task) const {
  QString prompt;

  prompt += QStringLiteral(
      "You are a file-manipulation agent. Your domain is the "
      "directory: %1\n"
      "\n"
      "You handle read, write, create, and list operations for files "
      "under your domain.\n"
      "\n"
      "You do not perform structural edits on existing files. If a "
      "task requires an insert, replace, or delete on the content of "
      "an existing file, you delegate it to a scoped edit agent.\n"
      "\n"
      "Creating a new file is NOT a structural edit. Use write_file "
      "for any task that creates a file that does not exist yet. Do "
      "not delegate a new-file creation to a scoped edit agent, "
      "because a scoped edit agent needs an existing file to edit.\n"
      "\n"
      "You may call multiple tools to complete a task. Each response "
      "is exactly one JSON object naming one tool call or one "
      "terminal action. After a tool call you will be asked again, "
      "with the result of that call shown below, until you answer "
      "with \"done\" or \"fail\". Do not repeat a tool call whose "
      "result is already shown.\n"
      "\n"
      "Respond with exactly one JSON object. No markdown fences. No "
      "explanatory text. No trailing commentary of any kind.\n"
      "\n"
      "The object has exactly one of these shapes:\n"
      "\n"
      "  {\"action\": \"call_tool\",\n"
      "   \"tool\": \"write_file\",\n"
      "   \"args\": {\"path\": \"...\", \"content\": \"...\"}}\n"
      "\n"
      "  {\"action\": \"call_tool\",\n"
      "   \"tool\": \"read_file\",\n"
      "   \"args\": {\"path\": \"...\"}}\n"
      "\n"
      "  {\"action\": \"call_tool\",\n"
      "   \"tool\": \"list_directory\",\n"
      "   \"args\": {\"path\": \"...\"}}\n"
      "\n"
      "  {\"action\": \"call_tool\",\n"
      "   \"tool\": \"create_directory\",\n"
      "   \"args\": {\"path\": \"...\"}}\n"
      "\n"
      "  {\"action\": \"delegate_scoped_edit\",\n"
      "   \"file\": \"<path relative to the session output folder of "
      "an EXISTING file>\",\n"
      "   \"instruction\": \"...\"}\n"
      "\n"
      "  {\"action\": \"done\", \"summary\": \"...\"}\n"
      "\n"
      "  {\"action\": \"fail\", \"reason\": \"...\"}\n"
      "\n"
      "Note: the JSON string for write_file's content MUST be "
      "properly escaped. Newlines must be written as \\n, quotes as "
      "\\\", and backslashes as \\\\. An unescaped newline inside a "
      "JSON string will make the whole response invalid and the task "
      "will fail.\n");

  if (!m_taskToolCalls.isEmpty()) {
    prompt += QStringLiteral(
        "\n"
        "Tool calls already made for this task:\n");

    for (const QJsonObject &call : std::as_const(m_taskToolCalls)) {
      const QString tool = call.value(QStringLiteral("tool")).toString();
      const QJsonObject args = call.value(QStringLiteral("args")).toObject();
      const bool ok = call.value(QStringLiteral("ok")).toBool();
      const QString result = call.value(QStringLiteral("result")).toString();

      if (tool == QStringLiteral("__parse_error__")) {
        prompt += QStringLiteral(
            "  (previous reply was not valid JSON; it was rejected)\n");
        prompt += QStringLiteral("    %1\n").arg(result);
        continue;
      }

      prompt += QStringLiteral("  %1(%2) -> %3\n")
                    .arg(tool,
                         QString::fromUtf8(QJsonDocument(args).toJson(
                             QJsonDocument::Compact)),
                         ok ? QStringLiteral("ok") : QStringLiteral("error"));

      prompt += QStringLiteral("    %1\n").arg(result);
    }
  }

  prompt += QStringLiteral(
      "\n"
      "Recent history across tasks:\n%1\n"
      "\n"
      "Current task:\n%2\n")
      .arg(m_history.isEmpty() ? QStringLiteral("(none)")
                               : m_history.join(QChar('\n')),
           task.instruction);

  return prompt;
}