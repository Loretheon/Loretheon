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
#include "../../include/avatar/AvatarWidget.h"
#include "../../include/overseer/OverseerSessionManager.h"
#include "../../include/search/SearchService.h"
#include "../../include/text/DocumentArea.h"
#include "RetrievalLoop.h"
#include "inference/InferenceService.h"

#include <QDebug>
#include <QDir>
#include <QEventLoop>
#include <QJsonDocument>
#include <QJsonObject>

namespace {

constexpr double kTurnTemperature = 0.6;
constexpr int kTurnTimeoutMs = 120000;
constexpr int kMemoryRecallLimit = 4;

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

    if (!m_memoryIndex->load()) {
      qDebug() << "[LoreAssistant] No memory index on disk yet; "
                  "recall will be empty until it is rebuilt.";
    }

    m_memory->setMemoryIndex(m_memoryIndex);
  }

  if (m_config.inference) {
    m_memory->setInference(m_config.inference);
  }

  assistant::AssistantTools::installAll(*m_tools);

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

  if (m_turnActive && m_config.inference) {
    m_config.inference->abortChatRequest(m_turnToken);
  }

  m_turnActive = false;
  m_turnMessages = QJsonArray();
  m_turnReplyBuffer.clear();

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

void LoreAssistant::handleUserMessage(const QString &text) {
  QString trimmed = text.trimmed();

  if (trimmed.isEmpty()) {
    return;
  }

  if (!m_started) {
    emit assistantStatus(tr("The assistant is not started."));
    return;
  }

  if (!m_config.inference || !m_config.inference->isLlmReady()) {
    emit assistantStatus(tr("No language model is ready."));
    emit assistantTurnFinished();
    return;
  }

  if (m_turnActive) {
    emit assistantStatus(tr("One moment — I am still answering."));
    return;
  }

  // A pending result waiting to be appended to the next user message
  // joins this one now.
  if (!m_pendingForUserMessage.isEmpty()) {
    const QString joined =
        m_pendingForUserMessage.join(QStringLiteral("\n\n"));

    m_pendingForUserMessage.clear();

    trimmed += QStringLiteral("\n\n---\n\n");
    trimmed += joined;
  }

  m_turnReplyBuffer.clear();
  m_turnActive = true;
  m_turnMessages = buildMessages(trimmed);
  m_toolRoundsRemaining = 3;

  const QJsonArray tools = m_tools ? m_tools->schemas() : QJsonArray();

  m_turnToken = m_config.inference->sendChatRequest(
      m_turnMessages, QString(), kTurnTemperature, kTurnTimeoutMs,
      QString(), QJsonObject(), tools, QString());
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
      "You are Lore. You are speaking to the user through a small "
      "chat panel. Keep replies short.\n"
      "\n"
      "The material above is everything you know about yourself and "
      "about the user: your identity, what you know about the user, "
      "and what you know about your own state. That material is "
      "already loaded. It is not a reference to consult. It is what "
      "you know right now, in this moment.\n"
      "\n"
      "If the user asks you something that is answered by what is "
      "written above — their name, their preferences, anything you "
      "have been told before — answer directly from it. Do not "
      "search. Do not say you do not know. The answer is in front "
      "of you.\n"
      "\n"
      "You do not do work yourself. You have four tools and you use "
      "them.\n"
      "\n"
      "- search: ask a question of the user's notes. Use this only "
      "when the user asks about something that might be written "
      "somewhere in their notes and is not already known to you. "
      "Never use it for things about the user themselves, or for "
      "anything already written above.\n"
      "\n"
      "- delegate: hand a task to the worker. The worker creates and "
      "modifies notes. Use this when the user asks you to write, "
      "rewrite, restructure, or produce something. The task runs in "
      "the background; you will be told when it is done. Reply to "
      "the user with a short acknowledgement and stop.\n"
      "\n"
      "- remember_fact: remember something durable. Use scope 'user' "
      "when the user tells you something about themselves. Use "
      "scope 'self' when you learn something about your own state. "
      "Use scope 'memory' with a topic for anything else. Write the "
      "fact immediately; the user does not need to approve it.\n"
      "\n"
      "- speak: say something aloud through the user's speakers. Use "
      "this when the user asks you to read something, or when a "
      "spoken line is more natural than text.\n"
      "\n"
      "Cite the notes you use. When the search or the worker gives "
      "you a source, mention it inline as [1], [2], and so on, in "
      "the order the sources were given. Do not type file paths; "
      "the application resolves the markers.\n"
      "\n"
      "When a background task finishes, its result is delivered to "
      "you as a system message. Treat it as something you just "
      "learned.");

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

  context.editor = m_config.documentArea
                       ? m_config.documentArea->currentEditor()
                       : nullptr;

  context.overseerManager = m_config.overseerManager;
  context.promoter = m_config.promoter;
  context.scopeIndex = m_config.scopeIndex;
  if (m_activity) {
    context.recentActivity = m_activity->recent();
  }

  // Called by tools that want the user to review before acting. For
  // now the assistant proceeds without asking; the seam is here so a
  // review card can be added without touching any tool.
  context.requestReview = nullptr;

  return context;
}

void LoreAssistant::onLlmDelta(const QUuid &token, const QString &text) {
  if (!m_turnActive || token != m_turnToken) {
    return;
  }

  m_turnReplyBuffer += text;

  emit assistantChunk(text);
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
    emit assistantStatus(tr("Too many tool rounds; stopping."));
    finishTurn();
    return;
  }

  runToolRound(toolCalls, m_turnMessages);
}

void LoreAssistant::onLlmError(const QUuid &token, const QString &error) {
  if (!m_turnActive || token != m_turnToken) {
    return;
  }

  emit assistantStatus(error);
  finishTurn();
}

void LoreAssistant::runToolRound(const QJsonArray &toolCalls,
                                 const QJsonArray &priorMessages) {
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
      } else {
        qWarning() << "[LoreAssistant] Tool arguments did not parse:"
                   << parseError.errorString();
      }
    }

    emit assistantStatus(tr("Running %1…").arg(name));

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
      emit assistantStatus(tr("%1 failed: %2").arg(name, payload));
    } else if (result.output.startsWith(
                   QStringLiteral("__lore_search__:"))) {
      // Intercept: run the search on the assistant's own loop, wait
      // for the answer, and return it as the tool result.
      const QString query =
          result.output.mid(QStringLiteral("__lore_search__:").size());

      payload = runSearchForTool(query);

      emit assistantStatus(tr("Search complete."));
    } else if (result.output.startsWith(
                   QStringLiteral("__lore_delegated__:"))) {
      // Intercept: the tool already submitted the request through the
      // manager. Record the request id and its policy, and tell the
      // model the task is running.
      const QString rest = result.output.mid(
          QStringLiteral("__lore_delegated__:").size());

      const int colon = rest.indexOf(QChar(':'));

      const QString requestId =
          colon < 0 ? rest : rest.left(colon);

      if (!requestId.isEmpty()) {
        m_pendingJobs.insert(requestId, m_completionPolicy);

        const QString sessionPart =
            colon < 0 ? QString() : rest.mid(colon + 1).trimmed();

        if (!sessionPart.isEmpty()) {
          m_jobSessions.insert(requestId, sessionPart);
        }
      }

      payload = tr("Task submitted. Request %1. You will be told when "
                   "it is done.")
                    .arg(requestId.left(8));

      emit assistantStatus(tr("Task submitted."));
                   } else if (result.output.startsWith(
                              QStringLiteral("__lore_needs_session__:"))) {
                     const QString instruction = result.output.mid(
                         QStringLiteral("__lore_needs_session__:").size());

                     QString listing = tr("The worker needs a session. Existing "
                                          "sessions:\n");

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
                     } else {
                       listing += tr("(the worker is not available)\n");
                     }

                     listing += tr("\nCall delegate again with a session name. Or "
                                   "call delegate with create=true, a name, and a "
                                   "short intentional description to make a new "
                                   "session.\n\nThe task was:\n");
                     listing += instruction;

                     payload = listing;

                     emit assistantStatus(tr("Choosing a session."));
                              }

    else {
      payload = result.output;
      emit assistantStatus(tr("%1 done.").arg(name));
    }

    QJsonObject toolMessage;
    toolMessage.insert(QStringLiteral("role"), QStringLiteral("tool"));
    toolMessage.insert(QStringLiteral("tool_call_id"), callId);
    toolMessage.insert(QStringLiteral("name"), name);
    toolMessage.insert(QStringLiteral("content"), payload);
    messages.append(toolMessage);
  }

  m_turnMessages = messages;
  m_turnReplyBuffer.clear();

  const QJsonArray tools = m_tools ? m_tools->schemas() : QJsonArray();

  m_turnToken = m_config.inference->sendChatRequest(
      m_turnMessages, QString(), kTurnTemperature, kTurnTimeoutMs,
      QString(), QJsonObject(), tools, QString());
}

QString LoreAssistant::runSearchForTool(const QString &query) {
  if (!m_config.search || !m_config.inference) {
    return QStringLiteral("Search is unavailable.");
  }

  if (!m_searchLoop) {
    m_searchLoop = new RetrievalLoop(m_config.search,
                                     m_config.inference, this);
  }

  if (m_searchInFlight) {
    m_searchLoop->cancel();
  }

  m_searchAnswerBuffer.clear();
  m_searchInFlight = true;

  // Run synchronously from the tool's point of view. RetrievalLoop is
  // asynchronous through the network, so we spin a local event loop
  // until it finishes. The tool runs inside a turn, so blocking here
  // is safe: the user has already submitted and is waiting.
  QEventLoop loop;

  QMetaObject::Connection finishedConn = connect(
      m_searchLoop, &RetrievalLoop::finished, &loop,
      [this, &loop](const QString &answer) {
        m_searchAnswerBuffer = answer;
        m_searchInFlight = false;
        loop.quit();
      });

  QMetaObject::Connection failedConn = connect(
      m_searchLoop, &RetrievalLoop::failed, &loop,
      [this, &loop](const QString &reason) {
        m_searchAnswerBuffer =
            QStringLiteral("Search failed: ") + reason;
        m_searchInFlight = false;
        loop.quit();
      });

  QMetaObject::Connection chunkConn = connect(
      m_searchLoop, &RetrievalLoop::answerChunk, &loop,
      [this](const QString &chunk) { m_searchAnswerBuffer += chunk; });

  m_searchLoop->start(query);
  loop.exec();

  disconnect(finishedConn);
  disconnect(failedConn);
  disconnect(chunkConn);

  const QString answer = m_searchAnswerBuffer;
  m_searchAnswerBuffer.clear();

  if (answer.isEmpty()) {
    return QStringLiteral("No notes matched that query.");
  }

  return answer;
}

void LoreAssistant::finishTurn() {
  m_turnActive = false;
  m_turnMessages = QJsonArray();
  m_toolRoundsRemaining = 0;

  m_lastReply = m_turnReplyBuffer;
  m_turnReplyBuffer.clear();

  const Settings::AssistantSettings settings =
      Settings::getAssistantSettings();

  if (settings.speakResponses && !m_lastReply.isEmpty()) {
    say(m_lastReply);
  }

  emit assistantTurnFinished();
}

void LoreAssistant::onOverseerRequestFinished(
    const QString &sessionName, const QString &requestId, bool ok,
    const QString &summary, const QString &filePath) {
  Q_UNUSED(sessionName);

  if (!m_pendingJobs.contains(requestId)) {
    // A request the user submitted themselves, not one Lore started.
    return;
  }

  const CompletionPolicy policy = m_pendingJobs.take(requestId);

  if (!ok) {
    const QString line = tr("A task I started failed: %1").arg(summary);
    emit assistantStatus(line);
    emit jobCompleted(line);
    return;
  }

  QString line = summary;

  if (!filePath.isEmpty()) {
    line += QStringLiteral(" ");
    line += tr("(%1)").arg(QFileInfo(filePath).fileName());
  }

  CompletionPolicy effective = policy;

  if (policy == CompletionPolicy::Automatic) {
    // If the user is mid-turn, queue it. If the assistant is idle and
    // nothing else is queued, paste it in chat. If something is
    // already queued, keep queueing.
    if (m_turnActive) {
      effective = CompletionPolicy::FeedToQueue;
    } else if (m_pendingForPrompt.isEmpty() &&
               m_pendingForUserMessage.isEmpty()) {
      effective = CompletionPolicy::PasteInChat;
    } else {
      effective = CompletionPolicy::FeedToQueue;
    }
  }

  switch (effective) {
  case CompletionPolicy::PasteInChat:
    emit assistantChunk(QStringLiteral("\n\n") + line);
    emit jobCompleted(line);
    break;

  case CompletionPolicy::FeedToQueue:
    m_pendingForPrompt.append(line);
    emit assistantStatus(tr("A background task finished."));
    break;

  case CompletionPolicy::AppendToNextUserMessage:
    m_pendingForUserMessage.append(line);
    emit assistantStatus(tr("A background task finished."));
    break;

  case CompletionPolicy::Automatic:
    // handled above; not reached
    break;
  }
}

void LoreAssistant::applyCompletionPolicy(const QString &summary) {
  Q_UNUSED(summary);
  // Reserved for future use. The policy is applied in
  // onOverseerRequestFinished, where the request id is known.
}