#include "../../include/assistant/LoreAssistant.h"

#include "../../include/app/DocumentManager.h"
#include "../../include/app/NotificationService.h"
#include "../../include/app/Settings.h"
#include "../../include/assistant/AssistantActivity.h"
#include "../../include/assistant/AssistantMemory.h"
#include "../../include/assistant/AssistantProfile.h"
#include "../../include/assistant/AssistantToolRegistry.h"
#include "../../include/assistant/MemoryIndex.h"
#include "../../include/assistant/SpeechAnimator.h"
#include "../../include/assistant/tools/AssistantTools.h"
#include "../../include/avatar/AvatarWidget.h"
#include "../../include/search/SearchService.h"
#include "../../include/text/DocumentArea.h"
#include "inference/InferenceService.h"

#include <QDebug>
#include <QDir>
#include <QJsonDocument>
#include <QJsonObject>

namespace {

constexpr double kTurnTemperature = 0.6;
constexpr int kTurnTimeoutMs = 120000;
constexpr int kMemoryRecallLimit = 4;

QString systemPromptFromProfile(AssistantProfile *profile) {
  if (!profile) {
    return QStringLiteral("You are Lore.");
  }

  QString prompt;

  const QString identity = profile->identity();
  const QString user = profile->user();
  const QString self = profile->self();

  if (!identity.isEmpty()) {
    prompt += identity;
    prompt += QStringLiteral("\n\n");
  }

  if (!user.isEmpty()) {
    prompt += QStringLiteral("What you know about the user:\n");
    prompt += user;
    prompt += QStringLiteral("\n\n");
  }

  if (!self.isEmpty()) {
    prompt += QStringLiteral("What you know about yourself:\n");
    prompt += self;
    prompt += QStringLiteral("\n\n");
  }

  prompt += QStringLiteral(
      "You are speaking to the user through a small chat window. "
      "Keep replies short. If a tool would help, call it. Do not "
      "narrate your tool use; the application shows the user that a "
      "tool is running.");

  return prompt;
}

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
  }

  if (m_config.inference) {
    m_memoryIndex = new MemoryIndex(m_config.inference, this);
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

void LoreAssistant::handleUserMessage(const QString &text) {
  const QString trimmed = text.trimmed();

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

  m_turnReplyBuffer.clear();

  m_turnActive = true;
  m_turnMessages = buildMessages(trimmed);
  m_toolRoundsRemaining = 1;

  const QJsonArray tools = m_tools ? m_tools->schemas() : QJsonArray();

  m_turnToken = m_config.inference->sendChatRequest(
      m_turnMessages, QString(), kTurnTemperature, kTurnTimeoutMs,
      QString(), QJsonObject(), tools, QString());
}

QJsonArray LoreAssistant::buildMessages(const QString &userText) const {
  QJsonArray messages;

  QJsonObject system;
  system.insert(QStringLiteral("role"), QStringLiteral("system"));

  QString content = systemPromptFromProfile(m_profile);

  const QString recalled = memoryContext(m_memory, userText);

  if (!recalled.isEmpty()) {
    content += QStringLiteral("\n\n");
    content += recalled;
  }

  system.insert(QStringLiteral("content"), content);
  messages.append(system);

  QJsonObject user;
  user.insert(QStringLiteral("role"), QStringLiteral("user"));
  user.insert(QStringLiteral("content"), userText);
  messages.append(user);

  return messages;
}

assistant::AssistantToolContext LoreAssistant::buildToolContext() const {
  assistant::AssistantToolContext context;

  context.documents = m_config.documents;
  context.inference = m_config.inference;
  context.search = m_config.search;
  context.memory = m_memory;
  context.profile = m_profile;
  context.avatar = m_config.avatar;

  // The editor is now reachable. DocumentArea is passed in through
  // Config and its currentEditor() is the focused editor, or null if
  // no document is open.
  context.editor = m_config.documentArea
                       ? m_config.documentArea->currentEditor()
                       : nullptr;

  if (m_activity) {
    context.recentActivity = m_activity->recent();
  }

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

  NotificationService::instance().error(tr("Assistant"), error);

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

    emit toolStarted(name);
    emit assistantStatus(tr("Running %1…").arg(name));

    assistant::AssistantTool::Result result;

    if (m_tools) {
      result = m_tools->execute(name, arguments, context);
    } else {
      result.ok = false;
      result.error = QStringLiteral("No tool registry.");
    }

    QString payload;

    if (result.ok) {
      payload = result.output;
      emit toolFinished(name, true, payload.left(160));
      emit assistantStatus(tr("%1 done.").arg(name));
    } else {
      payload = result.error.isEmpty()
                    ? QStringLiteral("Tool failed.")
                    : result.error;
      emit toolFinished(name, false, payload);
      emit assistantStatus(tr("%1 failed: %2").arg(name, payload));

      NotificationService::instance().warning(
          tr("Assistant tool: %1").arg(name), payload);
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