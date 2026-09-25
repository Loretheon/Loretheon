#include "../../include/assistant/LoreAssistant.h"

#include "../../include/app/DocumentManager.h"
#include "../../include/app/Settings.h"
#include "../../include/assistant/AssistantActivity.h"
#include "../../include/assistant/AssistantMemory.h"
#include "../../include/assistant/AssistantProfile.h"
#include "../../include/assistant/AssistantToolRegistry.h"
#include "../../include/assistant/MemoryIndex.h"
#include "../../include/assistant/SpeechAnimator.h"
#include "../../include/assistant/tools/AssistantTools.h"
#include "../../include/assistant/tools/NoteTools.h"
#include "../../include/avatar/AvatarWidget.h"
#include "../../include/overseer/OverseerSessionManager.h"
#include "../../include/search/NotePromoter.h"
#include "../../include/search/RetrievalLoop.h"
#include "../../include/search/ScopeIndex.h"
#include "../../include/search/SearchService.h"
#include "../../include/text/DocumentArea.h"
#include "inference/InferenceService.h"

#include <QDebug>
#include <QDir>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTimer>

namespace {

constexpr double kTurnTemperature = 0.6;
constexpr int kTurnTimeoutMs = 120000;
constexpr int kMemoryRecallLimit = 4;
constexpr int kJobWaitTimeoutMs = 180000;

QString memoryContext(AssistantMemory *memory, const QString &query) {
  if (!memory) {
    return {};
  }

  const QStringList recalled = memory->recall(query, kMemoryRecallLimit);

  if (recalled.isEmpty()) {
    return {};
  }

  return QStringLiteral("Relevant memories:\n") +
         recalled.join(QStringLiteral("\n---\n"));
}

} // namespace

LoreAssistant::LoreAssistant(const Config &config, QObject *parent)
    : QObject(parent), m_config(config) {
  m_profile = new AssistantProfile();
  m_memory = new AssistantMemory();
  m_activity = new AssistantActivity(this);
  m_tools = new assistant::AssistantToolRegistry();

  m_animator = new SpeechAnimator(m_config.inference, m_config.avatar, this);

  if (m_config.inference) {
    connect(m_config.inference, &InferenceService::llmDelta, this,
            &LoreAssistant::onLlmDelta);
    connect(m_config.inference, &InferenceService::llmFinished, this,
            &LoreAssistant::onLlmFinished);
    connect(m_config.inference, &InferenceService::llmToolCalls, this,
            &LoreAssistant::onLlmToolCalls);
    connect(m_config.inference, &InferenceService::llmError, this,
            &LoreAssistant::onLlmError);

    m_memoryIndex = new MemoryIndex(m_config.inference, this);
  }

  if (m_config.overseerManager) {
    connect(m_config.overseerManager,
            &OverseerSessionManager::requestFinished, this,
            &LoreAssistant::onOverseerRequestFinished);
  }
}

LoreAssistant::~LoreAssistant() {
  stop();

  delete m_tools;
  delete m_memory;
  delete m_profile;
}

bool LoreAssistant::start() {
  if (m_started) {
    return true;
  }

  if (m_config.root.isEmpty()) {
    qWarning() << "[LoreAssistant] No assistant root configured.";
    return false;
  }

  QDir().mkpath(m_config.root);

  m_profile->setRoot(m_config.root);

  if (!m_profile->load()) {
    qWarning() << "[LoreAssistant] Profile could not be loaded from"
               << m_config.root;
    return false;
  }

  m_memory->setRoot(m_config.root);

  if (!m_memory->ensureRoot()) {
    qWarning() << "[LoreAssistant] Memory tree could not be created under"
               << m_config.root;
    return false;
  }

  if (m_memoryIndex) {
    m_memoryIndex->setMemoryRoot(
        QDir(m_config.root).filePath(QStringLiteral("memories")));

    const bool loaded = m_memoryIndex->load();

    if (!loaded) {
      qDebug() << "[LoreAssistant] Building memory index.";
      m_memoryIndex->rebuild();
    }

    m_memory->setMemoryIndex(m_memoryIndex);
  }

  if (m_config.inference) {
    m_memory->setInference(m_config.inference);
  }

  assistant::AssistantTools::installAll(*m_tools);
  assistant::NoteTools::installAll(*m_tools);

  m_activity->startBatchTimer();

  m_started = true;

  qDebug() << "[LoreAssistant] Started. Root:" << m_config.root
           << "Tools:" << !m_tools->isEmpty();

  return true;
}

void LoreAssistant::stop() {
  if (!m_started) {
    return;
  }

  abortAll();

  m_activity->stopBatchTimer();

  if (m_animator) {
    m_animator->reset();
  }

  m_started = false;
}

void LoreAssistant::say(const QString &text) {
  if (text.isEmpty() || !m_config.inference) {
    return;
  }

  m_config.inference->speak(text);

  emit assistantSaid(text);
}

void LoreAssistant::setCompletionPolicy(CompletionPolicy policy) {
  m_completionPolicy = policy;
}

const LoreAssistant::Job *LoreAssistant::job(const QString &jobId) const {
  auto it = m_jobs.constFind(jobId);
  return it == m_jobs.constEnd() ? nullptr : &it.value();
}

QStringList LoreAssistant::jobIds() const { return m_jobs.keys(); }

void LoreAssistant::handleUserMessage(const QString &text) {
  const QString trimmed = text.trimmed();

  if (trimmed.isEmpty()) {
    return;
  }

  if (!m_started) {
    emit statusMessage(tr("The assistant is not started."));
    return;
  }

  if (!m_config.inference || !m_config.inference->isLlmReady()) {
    emit statusMessage(tr("No language model is ready."));
    return;
  }

  if (m_turnActive) {
    emit statusMessage(tr("One moment — I am still answering."));
    return;
  }

  // Reset the reply node at the start of a new exchange only.
  m_activeReplyNode.clear();
  m_turnReplyBuffer.clear();

  m_turnActive = true;
  m_toolRoundsRemaining = 8;

  m_turnMessages = buildMessages(trimmed);

  const QJsonArray tools = m_tools ? m_tools->schemas() : QJsonArray();

  m_turnToken = m_config.inference->sendChatRequest(
      m_turnMessages, QString(), kTurnTemperature, kTurnTimeoutMs,
      QString(), QJsonObject(), tools, QString());

  emit statusChanged(tr("Thinking"));
}

QJsonArray LoreAssistant::buildMessages(const QString &userText) {
  QJsonArray messages;

  QJsonObject system;
  system.insert(QStringLiteral("role"), QStringLiteral("system"));

  QString content = systemPrompt();

  const QString recalled = memoryContext(m_memory, userText);

  if (!recalled.isEmpty()) {
    content += QStringLiteral("\n\n");
    content += recalled;
  }

  if (!m_pendingForPrompt.isEmpty()) {
    content += QStringLiteral("\n\nBackground results since your last "
                              "message:\n");
    content += m_pendingForPrompt.join(QStringLiteral("\n---\n"));
    m_pendingForPrompt.clear();
  }

  system.insert(QStringLiteral("content"), content);
  messages.append(system);

  QJsonObject user;
  user.insert(QStringLiteral("role"), QStringLiteral("user"));
  user.insert(QStringLiteral("content"), userText);
  messages.append(user);

  return messages;
}

QString LoreAssistant::systemPrompt() const {
  QString prompt;

  const QString identity = m_profile ? m_profile->identity() : QString();
  const QString user = m_profile ? m_profile->user() : QString();
  const QString self = m_profile ? m_profile->self() : QString();

  if (!identity.trimmed().isEmpty()) {
    prompt += identity.trimmed();
    prompt += QStringLiteral("\n\n");
  }

  if (!user.trimmed().isEmpty()) {
    prompt += QStringLiteral("## What you know about the user\n\n");
    prompt += user.trimmed();
    prompt += QStringLiteral("\n\n");
  }

  if (!self.trimmed().isEmpty()) {
    prompt += QStringLiteral("## What you know about yourself\n\n");
    prompt += self.trimmed();
    prompt += QStringLiteral("\n\n");
  }

  prompt += QStringLiteral(
      "## Runtime\n"
      "\n"
      "You are speaking to the user through a small chat panel. "
      "Keep replies short. The material above is what you already "
      "know; it is not a reference to consult.\n"
      "\n"
      "## Jobs and the cache\n"
      "\n"
      "Anything that reaches outside this conversation — a search, "
      "a delegate, a promote — is a job. Jobs run in the background. "
      "A job returns a job id immediately and does not block. You "
      "can start a job and keep talking.\n"
      "\n"
      "The result of a job is stored in a cache keyed by its id. "
      "You are not told the result automatically. If you need it, "
      "call read_job with the id. If the job is still running, the "
      "call waits; the user can abort the wait at any time.\n"
      "\n"
      "## Tools\n"
      "\n"
      "- search: start a search job. Returns the job id. Does not "
      "return the result. Call read_job to read it.\n"
      "\n"
      "- read_job: read the result of a job by id. Returns "
      "immediately if the job is done. Waits if it is still "
      "running.\n"
      "\n"
      "- delegate: hand a task to the worker. Returns the job id.\n"
      "\n"
      "- promote_note: copy a file the worker produced into the "
      "notes.\n"
      "\n"
      "- list_notes, read_note, write_note, edit_note, delete_note: "
      "operate on the user's notes folder.\n"
      "\n"
      "- remember_fact: write a durable fact about the user or "
      "about yourself.\n"
      "\n"
      "- speak: say something aloud.\n"
      "\n"
      "## Failure\n"
      "\n"
      "If a tool fails, do not retry it with the same arguments. "
      "Tell the user what failed and stop.");

  return prompt;
}

assistant::AssistantToolContext LoreAssistant::buildToolContext() {
  assistant::AssistantToolContext context;

  context.documents = m_config.documents;
  context.inference = m_config.inference;
  context.search = m_config.search;
  context.memory = m_memory;
  context.profile = m_profile;
  context.avatar = m_config.avatar;
  context.overseerManager = m_config.overseerManager;
  context.promoter = m_config.promoter;
  context.scopeIndex = m_config.scopeIndex;
  context.notesRoot = m_config.notesRoot;
  context.assistant = this;

  context.editor = m_config.documentArea
                       ? m_config.documentArea->currentEditor()
                       : nullptr;

  if (m_activity) {
    context.recentActivity = m_activity->recent();
  }

  context.requestReview = nullptr;

  return context;
}

void LoreAssistant::onLlmDelta(const QUuid &token, const QString &text) {
  if (!m_turnActive || token != m_turnToken) {
    return;
  }

  if (m_activeReplyNode.isEmpty()) {
    m_activeReplyNode =
        QUuid::createUuid().toString(QUuid::WithoutBraces);

    emit assistantReplyStarted(m_activeReplyNode);
  }

  m_turnReplyBuffer += text;

  emit assistantChunk(m_activeReplyNode, text);
}

void LoreAssistant::onLlmFinished(const QUuid &token) {
  if (!m_turnActive || token != m_turnToken) {
    return;
  }

  finishTurn();
}

void LoreAssistant::onLlmToolCalls(const QUuid &token,
                                   const QJsonArray &toolCalls) {
  if (!m_turnActive || token != m_turnToken) {
    return;
  }

  if (m_toolRoundsRemaining <= 0) {
    emit statusMessage(tr("Too many tool rounds; stopping."));
    finishTurn();
    return;
  }

  runToolRound(toolCalls, m_turnMessages);
}

void LoreAssistant::onLlmError(const QUuid &token, const QString &error) {
  if (!m_turnActive || token != m_turnToken) {
    return;
  }

  emit statusMessage(error);
  finishTurn();
}

void LoreAssistant::runToolRound(const QJsonArray &toolCalls,
                                 const QJsonArray &priorMessages) {

  if (m_activeReplyNode.isEmpty()) {
    m_activeReplyNode =
        QUuid::createUuid().toString(QUuid::WithoutBraces);

    emit assistantReplyStarted(m_activeReplyNode);
  }

  
  --m_toolRoundsRemaining;

  const assistant::AssistantToolContext context = buildToolContext();

  QJsonArray messages = priorMessages;

  QJsonObject assistantMessage;
  assistantMessage.insert(QStringLiteral("role"),
                          QStringLiteral("assistant"));
  assistantMessage.insert(QStringLiteral("tool_calls"), toolCalls);
  assistantMessage.insert(QStringLiteral("content"), QString());
  messages.append(assistantMessage);

  for (const QJsonValue &value : toolCalls) {
    if (!value.isObject()) {
      continue;
    }

    const QJsonObject call = value.toObject();

    const QString callId =
        call.value(QStringLiteral("id")).toString();

    const QJsonObject function =
        call.value(QStringLiteral("function")).toObject();

    const QString name =
        function.value(QStringLiteral("name")).toString();

    const QString argumentsText =
        function.value(QStringLiteral("arguments")).toString();

    QJsonObject arguments;

    if (!argumentsText.trimmed().isEmpty()) {
      QJsonParseError parseError;
      const QJsonDocument parsed = QJsonDocument::fromJson(
          argumentsText.toUtf8(), &parseError);

      if (parseError.error == QJsonParseError::NoError &&
          parsed.isObject()) {
        arguments = parsed.object();
      }
    }

    emit statusChanged(tr("Running %1").arg(name));

    assistant::AssistantTool::Result result;

    if (m_tools) {
      result = m_tools->execute(name, arguments, context);
    } else {
      result.ok = false;
      result.error = QStringLiteral("No tool registry.");
    }

    QString payload;

    if (!result.ok) {
      payload = result.error.isEmpty()
                    ? QStringLiteral("Tool failed.")
                    : result.error;
    } else if (result.output.startsWith(
                   QStringLiteral("__job_search__:"))) {
      const QString query =
          result.output.mid(QStringLiteral("__job_search__:").size());

      const QString jobId = startSearchJob(query, m_activeReplyNode);

      payload = tr("Job started. id=%1. Read it with read_job when you "
                   "need it.").arg(jobId);
    } else if (result.output.startsWith(
                   QStringLiteral("__job_delegated__:"))) {
      const QString rest = result.output.mid(
          QStringLiteral("__job_delegated__:").size());

      const int colon = rest.indexOf(QChar(':'));

      const QString requestId =
          colon < 0 ? rest : rest.left(colon);

      const QString sessionPart =
          colon < 0 ? QString() : rest.mid(colon + 1).trimmed();

      Job job;
      job.id = requestId;
      job.kind = ChatNode::Kind::JobDelegate;
      job.state = ChatNode::State::Running;
      job.nodeId = m_activeReplyNode;
      job.summary = tr("Delegated to %1").arg(
          sessionPart.isEmpty() ? tr("the worker") : sessionPart);
      job.createdAt = QDateTime::currentDateTime();
      job.updatedAt = job.createdAt;

      m_jobs.insert(job.id, job);
      m_jobPolicies.insert(job.id, m_completionPolicy);

      emit jobCreated(job.id, m_activeReplyNode, job.kind,
                      tr("Delegated"), job.summary);

      payload = tr("Job started. id=%1. Read it with read_job when you "
                   "need it.").arg(job.id);
    } else if (result.output.startsWith(
                   QStringLiteral("__job_promoted__:"))) {
      payload = result.output.mid(
          QStringLiteral("__job_promoted__:").size());
    } else if (result.output.startsWith(
                   QStringLiteral("__lore_needs_session__:"))) {
      const QString instruction = result.output.mid(
          QStringLiteral("__lore_needs_session__:").size());

      QString listing = tr("Existing sessions:\n");

      if (m_config.overseerManager) {
        const auto sessions =
            m_config.overseerManager->listSessionsWithDescriptions();

        if (sessions.isEmpty()) {
          listing += tr("(none yet)\n");
        } else {
          for (const auto &info : sessions) {
            listing += QStringLiteral("- %1 — %2\n")
                           .arg(info.name,
                                info.description.isEmpty()
                                    ? tr("(no description)")
                                    : info.description);
          }
        }
      }

      listing += tr("\nCall delegate again with a session name, or "
                    "with create=true, a name, and a description.");

      payload = listing;
    } else {
      payload = result.output;
    }

    QJsonObject toolMessage;
    toolMessage.insert(QStringLiteral("role"), QStringLiteral("tool"));
    toolMessage.insert(QStringLiteral("tool_call_id"), callId);
    toolMessage.insert(QStringLiteral("name"), name);
    toolMessage.insert(QStringLiteral("content"), payload);
    messages.append(toolMessage);
  }

  m_turnMessages = messages;

  // Do not clear the reply buffer or the reply node. The tool round is
  // a continuation of the same exchange; the final answer streams into
  // the same node. Only the buffer that becomes the final reply text
  // is reset, because the tool-call preamble is not part of the spoken
  // answer.
  m_turnReplyBuffer.clear();

  const QJsonArray tools = m_tools ? m_tools->schemas() : QJsonArray();

  m_turnToken = m_config.inference->sendChatRequest(
      m_turnMessages, QString(), kTurnTemperature, kTurnTimeoutMs,
      QString(), QJsonObject(), tools, QString());
}

QString LoreAssistant::startSearchJob(const QString &query,
                                      const QString &nodeId) {
  const QString jobId = QStringLiteral("search-%1")
                            .arg(m_nextJobOrdinal++);

  Job job;
  job.id = jobId;
  job.kind = ChatNode::Kind::JobSearch;
  job.state = ChatNode::State::Running;
  job.nodeId = nodeId;
  job.summary = query;
  job.createdAt = QDateTime::currentDateTime();
  job.updatedAt = job.createdAt;

  m_jobs.insert(jobId, job);
  m_jobPolicies.insert(jobId, m_completionPolicy);

  emit jobCreated(jobId, nodeId, ChatNode::Kind::JobSearch,
                  tr("Searching: %1").arg(query), QString());

  if (!m_config.search || !m_config.inference) {
    m_jobs[jobId].state = ChatNode::State::Failed;
    m_jobs[jobId].error = tr("Search is unavailable.");
    emit jobFailed(jobId, m_jobs[jobId].error);
    return jobId;
  }

  auto *loop = new RetrievalLoop(m_config.search,
                                 m_config.inference, this);

  m_searchLoops.insert(jobId, loop);
  m_searchBuffers.insert(jobId, QString());

  connect(loop, &RetrievalLoop::answerChunk, this,
          [this, jobId](const QString &chunk) {
            m_searchBuffers[jobId] += chunk;
          });

  connect(loop, &RetrievalLoop::finished, this,
          [this, jobId](const QString &answer) {
            onSearchJobFinished(jobId, answer);
          });

  connect(loop, &RetrievalLoop::failed, this,
          [this, jobId](const QString &reason) {
            onSearchJobFailed(jobId, reason);
          });

  loop->start(query);

  return jobId;
}

void LoreAssistant::onSearchJobFinished(const QString &jobId,
                                        const QString &result) {
  auto it = m_jobs.find(jobId);

  if (it == m_jobs.end()) {
    return;
  }

  const QString buffer = m_searchBuffers.take(jobId);
  const QString finalResult = result.isEmpty() ? buffer : result;

  it->state = ChatNode::State::Done;
  it->result = finalResult;
  it->updatedAt = QDateTime::currentDateTime();

  RetrievalLoop *loop = m_searchLoops.take(jobId);

  if (loop) {
    loop->deleteLater();
  }

  emit jobCompleted(jobId, finalResult);
  emit statusChanged(tr("Idle"));

  applyJobCompletion(*it);
}

void LoreAssistant::onSearchJobFailed(const QString &jobId,
                                      const QString &reason) {
  auto it = m_jobs.find(jobId);

  if (it == m_jobs.end()) {
    return;
  }

  it->state = ChatNode::State::Failed;
  it->error = reason;
  it->updatedAt = QDateTime::currentDateTime();

  RetrievalLoop *loop = m_searchLoops.take(jobId);

  if (loop) {
    loop->deleteLater();
  }

  m_searchBuffers.remove(jobId);

  emit jobFailed(jobId, reason);
  emit statusChanged(tr("Idle"));
}

void LoreAssistant::applyJobCompletion(const Job &job) {
  const CompletionPolicy policy =
      m_jobPolicies.value(job.id, CompletionPolicy::Automatic);

  CompletionPolicy effective = policy;

  if (policy == CompletionPolicy::Automatic) {
    if (m_turnActive) {
      effective = CompletionPolicy::FeedToQueue;
    } else if (m_pendingForPrompt.isEmpty() &&
               m_pendingForUserMessage.isEmpty()) {
      effective = CompletionPolicy::PasteInChat;
    } else {
      effective = CompletionPolicy::FeedToQueue;
    }
  }

  QString line = job.result;

  if (line.isEmpty()) {
    line = job.summary;
  }

  switch (effective) {
  case CompletionPolicy::PasteInChat:
    if (!m_activeReplyNode.isEmpty()) {
      emit assistantChunk(m_activeReplyNode,
                          QStringLiteral("\n\n") + line);
    }
    break;
  case CompletionPolicy::FeedToQueue:
    m_pendingForPrompt.append(line);
    emit statusMessage(tr("A background task finished."));
    break;
  case CompletionPolicy::AppendToNextUserMessage:
    m_pendingForUserMessage.append(line);
    emit statusMessage(tr("A background task finished."));
    break;
  case CompletionPolicy::Automatic:
    break;
  }
}

bool LoreAssistant::waitForJob(const QString &jobId, QString *resultOut,
                               QString *errorOut) {
  if (!m_jobs.contains(jobId)) {
    if (errorOut) {
      *errorOut = tr("No such job: %1").arg(jobId);
    }
    return false;
  }

  const Job *snapshot = job(jobId);

  if (snapshot && snapshot->isTerminal()) {
    if (snapshot->state == ChatNode::State::Done) {
      if (resultOut) {
        *resultOut = snapshot->result;
      }
      return true;
    }

    if (errorOut) {
      *errorOut = snapshot->error.isEmpty()
                      ? tr("Job failed or was cancelled.")
                      : snapshot->error;
    }
    return false;
  }

  QEventLoop loop;
  bool finished = false;

  auto connCompleted = connect(
      this, &LoreAssistant::jobCompleted, &loop,
      [&](const QString &id, const QString &result) {
        if (id != jobId) {
          return;
        }

        finished = true;
        if (resultOut) {
          *resultOut = result;
        }
        loop.quit();
      });

  auto connFailed = connect(
      this, &LoreAssistant::jobFailed, &loop,
      [&](const QString &id, const QString &error) {
        if (id != jobId) {
          return;
        }

        finished = true;
        if (errorOut) {
          *errorOut = error;
        }
        loop.quit();
      });

  QTimer::singleShot(kJobWaitTimeoutMs, &loop, [&]() {
    if (finished) {
      return;
    }

    if (errorOut) {
      *errorOut = tr("The job did not finish in time.");
    }
    loop.quit();
  });

  loop.exec();

  disconnect(connCompleted);
  disconnect(connFailed);

  return finished;
}

void LoreAssistant::abortJob(const QString &jobId) {
  auto it = m_jobs.find(jobId);

  if (it == m_jobs.end() || it->isTerminal()) {
    return;
  }

  RetrievalLoop *loop = m_searchLoops.take(jobId);

  if (loop) {
    loop->cancel();
    loop->deleteLater();
  }

  m_searchBuffers.remove(jobId);

  it->state = ChatNode::State::Cancelled;
  it->error = tr("Cancelled.");
  it->updatedAt = QDateTime::currentDateTime();

  emit jobFailed(jobId, it->error);
  emit statusChanged(tr("Idle"));
}

void LoreAssistant::abortAll() {
  if (m_turnActive && m_config.inference && !m_turnToken.isNull()) {
    m_config.inference->abortChatRequest(m_turnToken);
  }

  m_turnActive = false;
  m_turnMessages = QJsonArray();
  m_turnReplyBuffer.clear();
  m_activeReplyNode.clear();

  const QStringList ids = m_jobs.keys();

  for (const QString &id : ids) {
    abortJob(id);
  }
}

void LoreAssistant::finishTurn() {
  m_turnActive = false;
  m_turnMessages = QJsonArray();
  m_toolRoundsRemaining = 0;

  m_lastReply = m_turnReplyBuffer;
  m_turnReplyBuffer.clear();

  // The reply node is not cleared here. It is cleared when the user
  // sends the next message. That way every streamed delta in this
  // exchange — including deltas after a tool round — lands in the
  // same node and the tree does not create a second reply.

  emit assistantTurnFinished(m_activeReplyNode);
  emit statusChanged(tr("Idle"));

  const Settings::AssistantSettings settings =
      Settings::getAssistantSettings();

  if (settings.speakResponses && !m_lastReply.isEmpty()) {
    say(m_lastReply);
  }
}

void LoreAssistant::onOverseerRequestFinished(
    const QString &sessionName, const QString &requestId, bool ok,
    const QString &summary, const QString &filePath) {
  Q_UNUSED(sessionName);

  auto it = m_jobs.find(requestId);

  if (it == m_jobs.end()) {
    return;
  }

  QString line = summary;

  if (!filePath.isEmpty()) {
    line += QStringLiteral(" ");
    line += tr("(%1)").arg(QFileInfo(filePath).fileName());
  }

  if (!ok) {
    it->state = ChatNode::State::Failed;
    it->error = summary;
    it->updatedAt = QDateTime::currentDateTime();

    emit jobFailed(requestId, summary);
    emit statusChanged(tr("Idle"));
    return;
  }

  it->state = ChatNode::State::Done;
  it->result = line;
  it->updatedAt = QDateTime::currentDateTime();

  emit jobCompleted(requestId, line);
  emit statusChanged(tr("Idle"));

  applyJobCompletion(*it);
}