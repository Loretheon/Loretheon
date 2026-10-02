#include "../../include/assistant/LoreAssistant.h"

#include "../../include/app/DocumentManager.h"
#include "../../include/app/Settings.h"
#include "../../include/assistant/AssistantActivity.h"
#include "../../include/assistant/AssistantMemory.h"
#include "../../include/assistant/AssistantProfile.h"
#include "../../include/assistant/AssistantToolRegistry.h"
#include "../../include/assistant/ChatTree.h"
#include "../../include/assistant/ChatTreeStore.h"
#include "../../include/assistant/ConversationMode.h"
#include "../../include/assistant/MemoryIndex.h"
#include "../../include/assistant/NoteEditJob.h"
#include "../../include/assistant/ProfileEditJob.h"
#include "../../include/assistant/SpeechAnimator.h"
#include "../../include/assistant/tools/AssistantTools.h"
#include "../../include/assistant/tools/NoteTools.h"
#ifdef LORE_WITH_AVATAR
#include "../../include/avatar/AvatarWidget.h"
#endif
#include "../../include/ingest/IngestOptions.h"
#include "../../include/ingest/IngestService.h"
#include "../../include/overseer/OverseerSessionManager.h"
#include "../../include/search/NotePromoter.h"
#include "../../include/search/RetrievalLoop.h"
#include "../../include/search/ScopeIndex.h"
#include "../../include/search/SearchService.h"
#include "../../include/text/DocumentArea.h"
#include "../../include/voice/SpeechController.h"
#include "inference/InferenceService.h"

#include <QDebug>
#include <QDir>
#include <QEventLoop>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTextStream>
#include <QTimer>
#include <QUuid>

namespace {

constexpr double kTurnTemperature = 0.6;
constexpr int kTurnTimeoutMs = 120000;
constexpr int kMemoryRecallLimit = 4;
constexpr int kJobWaitTimeoutMs = 180000;
constexpr int kPasteThresholdChars = 2000;
constexpr int kRecentTailTokenBudget = 4000;

int estimateTokens(const QString &text) {
  return (text.size() + 3) / 4;
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

QString decodeMarkerInstruction(const QString &encoded) {
  QString result;
  result.reserve(encoded.size());

  for (int i = 0; i < encoded.size(); ++i) {
    const QChar ch = encoded.at(i);

    if (ch != QChar('\\') || i + 1 >= encoded.size()) {
      result.append(ch);
      continue;
    }

    const QChar next = encoded.at(i + 1);

    if (next == QChar('\\')) {
      result.append(QChar('\\'));
      ++i;
      continue;
    }

    if (next == QChar('n')) {
      result.append(QChar('\n'));
      ++i;
      continue;
    }

    result.append(ch);
  }

  return result;
}

QString makePasteId() {
  return QUuid::createUuid().toString(QUuid::WithoutBraces).left(8);
}

} // namespace

LoreAssistant::LoreAssistant(const Config &config, QObject *parent)
    : QObject(parent), m_config(config) {
  m_profile = new AssistantProfile();
  m_memory = new AssistantMemory();
  m_activity = new AssistantActivity(this);
  m_tools = new assistant::AssistantToolRegistry();
  m_chatStore = new ChatTreeStore(this);

  m_animator = new SpeechAnimator(m_config.inference, m_config.avatar, this);

  if (m_config.speech) {
    m_conversation = new ConversationMode(this, m_config.speech, this);
  }

  m_watchdog = new QTimer(this);
  m_watchdog->setSingleShot(false);
  m_watchdog->setInterval(5000);

  connect(m_watchdog, &QTimer::timeout, this,
          &LoreAssistant::onTurnWatchdog);

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
  QDir().mkpath(QDir(m_config.root).filePath(QStringLiteral("pastes")));

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

  if (m_chatStore) {
    m_chatStore->setRoot(m_config.root);
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

  m_watchdog->start();

  m_started = true;

  qDebug() << "[LoreAssistant] Started. Root:" << m_config.root
           << "Tools:" << !m_tools->isEmpty();

  return true;
}

void LoreAssistant::stop() {
  if (!m_started) {
    return;
  }

  if (m_conversation) {
    m_conversation->stop();
  }

  abortAll();

  m_activity->stopBatchTimer();

  if (m_watchdog) {
    m_watchdog->stop();
  }

  if (m_animator) {
    m_animator->reset();
  }

  m_started = false;
}

void LoreAssistant::say(const QString &text) {
  if (text.isEmpty()) {
    return;
  }

  if (m_config.speech) {
    m_config.speech->speakText(text);
  } else if (m_config.inference) {
    m_config.inference->speak(text);
  }

  emit assistantSaid(text);
}

const LoreAssistant::Job *LoreAssistant::job(const QString &jobId) const {
  auto it = m_jobs.constFind(jobId);
  return it == m_jobs.constEnd() ? nullptr : &it.value();
}

QStringList LoreAssistant::jobIds() const { return m_jobs.keys(); }

QString LoreAssistant::pastePath(const QString &id) const {
  if (m_config.root.isEmpty() || id.isEmpty())
    return {};

  return QDir(m_config.root)
      .filePath(QStringLiteral("pastes/%1.txt").arg(id));
}

QString LoreAssistant::writePaste(const QString &body) {
  if (m_config.root.isEmpty()) {
    return {};
  }

  QDir().mkpath(QDir(m_config.root).filePath(QStringLiteral("pastes")));

  const QString id = makePasteId();
  const QString path = pastePath(id);

  QSaveFile file(path);

  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    qWarning() << "[LoreAssistant] Cannot write paste:" << path;
    return {};
  }

  QTextStream stream(&file);
  stream.setEncoding(QStringConverter::Utf8);
  stream << body;
  stream.flush();

  if (stream.status() != QTextStream::Ok || !file.commit()) {
    qWarning() << "[LoreAssistant] Paste commit failed:" << path;
    return {};
  }

  return id;
}

bool LoreAssistant::canImport(const QString &sourcePath) const {
  if (!m_config.ingest || sourcePath.isEmpty()) {
    return false;
  }

  return m_config.ingest->canImport(sourcePath);
}

bool LoreAssistant::importToMemory(const QString &sourcePath,
                                   const QString &topicName) {
  if (!m_config.ingest) {
    emit statusMessage(tr("Import is not available."));
    return false;
  }

  const QFileInfo info(sourcePath);

  if (!info.exists() || !info.isFile()) {
    emit statusMessage(tr("No such file: %1").arg(sourcePath));
    return false;
  }

  if (!m_config.ingest->canImport(sourcePath)) {
    emit statusMessage(
        tr("No extractor for %1.").arg(info.fileName()));
    return false;
  }

  QString slug = topicName.trimmed();

  if (slug.isEmpty()) {
    slug = info.completeBaseName();
  }

  slug = AssistantMemory::slugify(slug);

  const QString destination =
      QDir(m_config.root)
          .filePath(QStringLiteral("memories/topics"));

  QDir().mkpath(destination);

  IngestOptions options;
  options.destinationFolder = destination;
  options.noteNameOverride = slug;
  options.writeProvenance = false;
  options.sectionPerPage = false;

  const QString fileName = info.fileName();

  m_config.ingest->import(
      sourcePath, options,
      [this, fileName](IngestService::Outcome outcome) {
        if (!outcome.ok()) {
          emit statusMessage(
              tr("Import failed: %1").arg(outcome.error));
          return;
        }

        if (m_memoryIndex) {
          m_memoryIndex->refreshFile(outcome.notePath);
        }

        emit knowledgeChanged();

        emit statusMessage(
            tr("Imported %1 into memory as %2.")
                .arg(fileName, QFileInfo(outcome.notePath).fileName()));
      },
      nullptr);

  return true;
}

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

  QString messageText = trimmed;

  if (trimmed.size() > kPasteThresholdChars) {
    const QString id = writePaste(trimmed);

    if (id.isEmpty()) {
      emit statusMessage(tr("Could not store the pasted text."));
      return;
    }

    messageText = QStringLiteral(
                      "[paste: %1 chars — id %2. Call read_paste(\"%2\") "
                      "to read it.]")
                      .arg(trimmed.size())
                      .arg(id);
  }

  Turn turn;
  turn.token = QUuid::createUuid();
  turn.replyNode = QUuid::createUuid().toString(QUuid::WithoutBraces);
  turn.messages = buildMessages(messageText);
  turn.toolRoundsRemaining = 8;

  m_turns.insert(turn.token, turn);
  m_nodeToTurn.insert(turn.replyNode, turn.token);
  m_turnUserText.insert(turn.token, trimmed);

  emit assistantReplyStarted(turn.replyNode);
  emit statusChanged(tr("Thinking"));

  sendTurnRequest(m_turns[turn.token]);
}

void LoreAssistant::setCurrentChat(const ChatTree *tree) {
  m_recentTail = QJsonArray();

  if (!tree)
    return;

  struct Pair {
    QString user;
    QString assistant;
  };

  QVector<Pair> pairs;

  const QVector<QString> roots = tree->roots();

  for (const QString &id : roots) {
    const ChatNode *node = tree->node(id);

    if (!node || node->kind != ChatNode::Kind::UserText)
      continue;

    Pair pair;
    pair.user = node->text;

    const int index = roots.indexOf(id);

    for (int j = index + 1; j < roots.size(); ++j) {
      const ChatNode *candidate = tree->node(roots.at(j));

      if (!candidate)
        continue;

      if (candidate->kind == ChatNode::Kind::AssistantText) {
        pair.assistant = candidate->text;
        break;
      }

      if (candidate->kind == ChatNode::Kind::UserText)
        break;
    }

    pairs.append(pair);
  }

  QVector<Pair> kept;
  int total = 0;

  for (int i = pairs.size() - 1; i >= 0; --i) {
    const Pair &pair = pairs.at(i);

    const int cost = estimateTokens(pair.user) +
                     estimateTokens(pair.assistant) + 16;

    if (total + cost > kRecentTailTokenBudget && !kept.isEmpty())
      break;

    kept.prepend(pair);
    total += cost;
  }

  for (const Pair &pair : kept) {
    QJsonObject userMessage;
    userMessage.insert(QStringLiteral("role"), QStringLiteral("user"));
    userMessage.insert(QStringLiteral("content"), pair.user);
    m_recentTail.append(userMessage);

    if (!pair.assistant.isEmpty()) {
      QJsonObject assistantMessage;
      assistantMessage.insert(QStringLiteral("role"),
                              QStringLiteral("assistant"));
      assistantMessage.insert(QStringLiteral("content"), pair.assistant);
      m_recentTail.append(assistantMessage);
    }
  }

  trimRecentTail();
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

  for (const QJsonValue &value : std::as_const(m_recentTail)) {
    if (value.isObject())
      messages.append(value.toObject());
  }

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
      "The recent conversation is included below as real messages. "
      "The oldest ones are trimmed when the tail grows too long. "
      "Anything further back is not in the prompt; if you need it, "
      "search your memories.\n"
      "\n"
      "## Pastes\n"
      "\n"
      "When the user pastes a large block of text, the message you "
      "receive carries a placeholder like "
      "[paste: 4821 chars — id a3f19c. Call read_paste(\"a3f19c\") "
      "to read it.] The body is on disk and is not sent to you "
      "automatically. Call read_paste only when you actually need "
      "the contents.\n"
      "\n"
      "## Jobs and the cache\n"
      "\n"
      "Anything that reaches outside this conversation — a search, "
      "a delegate, a promote, a profile edit — is a job. Jobs run in "
      "the background. A job returns a job id immediately and does "
      "not block. You can start a job and keep talking.\n"
      "\n"
      "The result of a job is stored in a cache keyed by its id. "
      "You are not told the result automatically. If you need it, "
      "call read_job with the id. If the job is still running, the "
      "call waits; the user can abort the wait at any time.\n"
      "\n"
      "The user may send another message while you are still "
      "answering. Treat each message as its own turn. Do not assume "
      "the turn you are in is the only one.\n"
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
      "- read_paste: read the full text of a paste by id.\n"
      "\n"
      "- delegate: hand a task to the worker. Returns the job id.\n"
      "\n"
      "- promote_note: copy a file the worker produced into the "
      "notes.\n"
      "\n"
      "- list_notes, read_note, write_note, edit_note, delete_note: "
      "operate on the user's notes folder.\n"
      "\n"
      "- edit_profile: edit one of your own files — identity.md, "
      "user.md, self.md, or a topic under memories/topics/. Use "
      "this to remember durable facts, revise what you know about "
      "the user, or adjust your own character. Returns the job id. "
      "There is no user review. Changes to identity.md take effect "
      "on your next turn.\n"
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
  context.root = m_config.root;
  context.assistant = this;

  context.editor = m_config.documentArea
                       ? m_config.documentArea->currentEditor()
                       : nullptr;

  if (m_activity) {
    context.recentActivity = m_activity->recent();
  }

  return context;
}

void LoreAssistant::sendTurnRequest(Turn &turn) {
  if (!m_config.inference) {
    return;
  }

  const QJsonArray tools = m_tools ? m_tools->schemas() : QJsonArray();

  const QUuid token = m_config.inference->sendChatRequest(
      turn.messages, QString(), kTurnTemperature, kTurnTimeoutMs,
      QString(), QJsonObject(), tools, QString());

  const QUuid oldToken = turn.token;

  if (oldToken != token) {
    Turn moved = m_turns.take(oldToken);
    moved.token = token;
    m_turns.insert(token, moved);
    m_nodeToTurn[moved.replyNode] = token;

    if (m_turnUserText.contains(oldToken)) {
      const QString userText = m_turnUserText.take(oldToken);
      m_turnUserText.insert(token, userText);
    }
  }
}

void LoreAssistant::onLlmDelta(const QUuid &token, const QString &text) {
  auto it = m_turns.find(token);

  if (it == m_turns.end()) {
    return;
  }

  Turn &turn = it.value();

  turn.replyBuffer += text;

  emit assistantChunk(turn.replyNode, text);
}

void LoreAssistant::onLlmFinished(const QUuid &token) {
  auto it = m_turns.find(token);

  if (it == m_turns.end()) {
    return;
  }

  finishTurn(token);
}

void LoreAssistant::onLlmToolCalls(const QUuid &token,
                                   const QJsonArray &toolCalls) {
  auto it = m_turns.find(token);

  if (it == m_turns.end()) {
    return;
  }

  Turn &turn = it.value();

  if (turn.toolRoundsRemaining <= 0) {
    emit statusMessage(tr("Too many tool rounds; stopping."));
    finishTurn(token);
    return;
  }

  runToolRound(token, toolCalls, turn.messages);
}

void LoreAssistant::onLlmError(const QUuid &token, const QString &error) {
  auto it = m_turns.find(token);

  if (it == m_turns.end()) {
    return;
  }

  emit statusMessage(error);
  finishTurn(token);
}

void LoreAssistant::runToolRound(const QUuid &turnToken,
                                 const QJsonArray &toolCalls,
                                 const QJsonArray &priorMessages) {
  auto it = m_turns.find(turnToken);

  if (it == m_turns.end()) {
    return;
  }

  Turn &turn = it.value();

  --turn.toolRoundsRemaining;

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

      const QString jobId = startSearchJob(query, turn.replyNode);

      payload = tr("Job started. id=%1. Read it with read_job when you "
                   "need it.").arg(jobId);
    } else if (result.output.startsWith(
                   QStringLiteral("__job_profile_edit__:"))) {
      const QString rest = result.output.mid(
          QStringLiteral("__job_profile_edit__:").size());

      const int newline = rest.indexOf(QChar('\n'));

      const QString path = newline < 0 ? rest : rest.left(newline);

      const QString encoded =
          newline < 0 ? QString() : rest.mid(newline + 1);

      const QString instruction = decodeMarkerInstruction(encoded);

      const QString jobId =
          startProfileEditJob(path, instruction, turn.replyNode);

      payload = tr("Job started. id=%1. Read it with read_job when you "
                   "need it.").arg(jobId);
    } else if (result.output.startsWith(
                   QStringLiteral("__lore_delegated__:"))) {
      const QString rest = result.output.mid(
          QStringLiteral("__lore_delegated__:").size());

      const int colon = rest.indexOf(QChar(':'));

      const QString requestId =
          colon < 0 ? rest : rest.left(colon);

      const QString sessionPart =
          colon < 0 ? QString() : rest.mid(colon + 1).trimmed();

      Job job;
      job.id = requestId;
      job.kind = ChatNode::Kind::JobDelegate;
      job.state = ChatNode::State::Running;
      job.nodeId = turn.replyNode;
      job.summary = tr("Delegated to %1").arg(
          sessionPart.isEmpty() ? tr("the worker") : sessionPart);
      job.createdAt = QDateTime::currentDateTime();
      job.updatedAt = job.createdAt;

      m_jobs.insert(job.id, job);

      emit jobCreated(job.id, turn.replyNode, job.kind,
                      tr("Delegated"), job.summary);

      payload = tr("Job started. id=%1. Read it with read_job when you "
                   "need it.").arg(job.id);
    } else if (result.output.startsWith(
                   QStringLiteral("__job_edit__:"))) {
      const QString rest = result.output.mid(
          QStringLiteral("__job_edit__:").size());

      const int newline = rest.indexOf(QChar('\n'));

      const QString notePath =
          newline < 0 ? rest : rest.left(newline);

      const QString instruction =
          newline < 0 ? QString() : rest.mid(newline + 1);

      const QString jobId =
          startNoteEditJob(notePath, instruction, turn.replyNode);

      payload = tr("Job started. id=%1. Read it with read_job when you "
                   "need it.").arg(jobId);
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

  const QUuid oldToken = turn.token;

  turn.messages = messages;
  turn.replyBuffer.clear();

  const Turn updated = turn;

  m_turns.erase(it);

  m_turns.insert(oldToken, updated);

  sendTurnRequest(m_turns[oldToken]);
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

QString LoreAssistant::startNoteEditJob(const QString &notePath,
                                        const QString &instruction,
                                        const QString &nodeId) {
  const QString jobId =
      QStringLiteral("edit-%1").arg(m_nextJobOrdinal++);

  Job job;
  job.id = jobId;
  job.kind = ChatNode::Kind::JobEdit;
  job.state = ChatNode::State::Running;
  job.nodeId = nodeId;
  job.summary = tr("Editing %1").arg(QFileInfo(notePath).fileName());
  job.createdAt = QDateTime::currentDateTime();
  job.updatedAt = job.createdAt;

  m_jobs.insert(jobId, job);

  emit jobCreated(jobId, nodeId, ChatNode::Kind::JobEdit,
                  tr("Editing: %1").arg(QFileInfo(notePath).fileName()),
                  instruction);

  if (!m_config.documents || !m_config.documentArea || !m_config.inference) {
    m_jobs[jobId].state = ChatNode::State::Failed;
    m_jobs[jobId].error = tr("The edit pipeline is unavailable.");
    emit jobFailed(jobId, m_jobs[jobId].error);
    return jobId;
  }

  auto *editJob = new NoteEditJob(m_config.documents, m_config.documentArea,
                                  m_config.inference, notePath, instruction,
                                  this);

  m_noteEditJobs.insert(jobId, editJob);

  connect(editJob, &NoteEditJob::finished, this,
          [this, jobId](const QString &summary) {
            auto it = m_jobs.find(jobId);

            if (it == m_jobs.end()) {
              return;
            }

            it->state = ChatNode::State::Done;
            it->result = summary;
            it->updatedAt = QDateTime::currentDateTime();

            m_noteEditJobs.remove(jobId);

            emit jobCompleted(jobId, summary);
          });

  connect(editJob, &NoteEditJob::failed, this,
          [this, jobId](const QString &reason) {
            auto it = m_jobs.find(jobId);

            if (it == m_jobs.end()) {
              return;
            }

            it->state = ChatNode::State::Failed;
            it->error = reason;
            it->updatedAt = QDateTime::currentDateTime();

            m_noteEditJobs.remove(jobId);

            emit jobFailed(jobId, reason);
          });

  editJob->start();

  return jobId;
}

QString LoreAssistant::startProfileEditJob(const QString &path,
                                           const QString &instruction,
                                           const QString &nodeId) {
  const QString jobId =
      QStringLiteral("profile-%1").arg(m_nextJobOrdinal++);

  Job job;
  job.id = jobId;
  job.kind = ChatNode::Kind::JobEdit;
  job.state = ChatNode::State::Running;
  job.nodeId = nodeId;
  job.summary = tr("Editing %1").arg(QFileInfo(path).fileName());
  job.createdAt = QDateTime::currentDateTime();
  job.updatedAt = job.createdAt;

  m_jobs.insert(jobId, job);

  emit jobCreated(jobId, nodeId, ChatNode::Kind::JobEdit,
                  tr("Editing: %1").arg(QFileInfo(path).fileName()),
                  instruction);

  if (!m_config.inference) {
    m_jobs[jobId].state = ChatNode::State::Failed;
    m_jobs[jobId].error = tr("The edit pipeline is unavailable.");
    emit jobFailed(jobId, m_jobs[jobId].error);
    return jobId;
  }

  auto *editJob = new ProfileEditJob(m_config.inference, path, instruction,
                                     this);

  m_profileEditJobs.insert(jobId, editJob);

  connect(editJob, &ProfileEditJob::finished, this,
          [this, jobId, path](const QString &summary) {
            auto it = m_jobs.find(jobId);

            if (it == m_jobs.end()) {
              return;
            }

            it->state = ChatNode::State::Done;
            it->result = summary;
            it->updatedAt = QDateTime::currentDateTime();

            m_profileEditJobs.remove(jobId);

            if (m_profile) {
              m_profile->reload();
            }

            if (path.contains(QStringLiteral("/memories/")) &&
                m_memoryIndex) {
              m_memoryIndex->refreshFile(path);
            }

            emit knowledgeChanged();
            emit jobCompleted(jobId, summary);
          });

  connect(editJob, &ProfileEditJob::failed, this,
          [this, jobId](const QString &reason) {
            auto it = m_jobs.find(jobId);

            if (it == m_jobs.end()) {
              return;
            }

            it->state = ChatNode::State::Failed;
            it->error = reason;
            it->updatedAt = QDateTime::currentDateTime();

            m_profileEditJobs.remove(jobId);

            emit jobFailed(jobId, reason);
          });

  editJob->start();

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

  if (NoteEditJob *editJob = m_noteEditJobs.take(jobId)) {
    editJob->abort();
  }

  if (ProfileEditJob *profileJob = m_profileEditJobs.take(jobId)) {
    profileJob->abort();
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
}

void LoreAssistant::abortTurn(const QString &replyNodeId) {
  auto nodeIt = m_nodeToTurn.find(replyNodeId);

  if (nodeIt == m_nodeToTurn.end()) {
    return;
  }

  abandonTurn(nodeIt.value());
}

void LoreAssistant::abortAll() {
  const QList<QUuid> tokens = m_turns.keys();

  for (const QUuid &token : tokens) {
    abandonTurn(token);
  }

  const QStringList ids = m_jobs.keys();

  for (const QString &id : ids) {
    abortJob(id);
  }
}

void LoreAssistant::recordRecentTurn(const QString &userText,
                                     const QString &assistantText) {
  if (userText.isEmpty() && assistantText.isEmpty()) {
    return;
  }

  QJsonObject userMessage;
  userMessage.insert(QStringLiteral("role"), QStringLiteral("user"));
  userMessage.insert(QStringLiteral("content"), userText);
  m_recentTail.append(userMessage);

  if (!assistantText.isEmpty()) {
    QJsonObject assistantMessage;
    assistantMessage.insert(QStringLiteral("role"),
                            QStringLiteral("assistant"));
    assistantMessage.insert(QStringLiteral("content"), assistantText);
    m_recentTail.append(assistantMessage);
  }

  trimRecentTail();
}

void LoreAssistant::trimRecentTail() {
  auto estimate = [](const QJsonArray &arr) {
    int total = 0;

    for (const QJsonValue &value : arr) {
      if (!value.isObject())
        continue;

      const QJsonObject obj = value.toObject();

      total += estimateTokens(
          obj.value(QStringLiteral("content")).toString());
      total += 8;
    }

    return total;
  };

  while (!m_recentTail.isEmpty() &&
         estimate(m_recentTail) > kRecentTailTokenBudget) {
    m_recentTail.removeFirst();

    if (!m_recentTail.isEmpty()) {
      const QJsonObject next = m_recentTail.first().toObject();

      if (next.value(QStringLiteral("role")).toString() ==
          QStringLiteral("assistant")) {
        m_recentTail.removeFirst();
      }
    }
  }
}

void LoreAssistant::finishTurn(const QUuid &turnToken) {
  auto it = m_turns.find(turnToken);

  if (it == m_turns.end()) {
    return;
  }

  Turn &turn = it.value();

  const QString replyText = turn.replyBuffer;

  if (!replyText.isEmpty()) {
    m_lastReply = replyText;
  }

  const QString replyNode = turn.replyNode;

  const QString userText = m_turnUserText.take(turnToken);

  if (!userText.isEmpty() || !replyText.isEmpty()) {
    recordRecentTurn(userText, replyText);
  }

  const bool conversationActive =
      m_conversation && m_conversation->isActive();

  const Settings::AssistantSettings settings =
      Settings::getAssistantSettings();

  if (!conversationActive && settings.speakResponses &&
      !m_lastReply.isEmpty()) {
    say(m_lastReply);
      }

  m_nodeToTurn.remove(replyNode);
  m_turns.erase(it);

  emit assistantTurnFinished(replyNode);

  if (m_turns.isEmpty()) {
    emit statusChanged(tr("Idle"));
  }
}

void LoreAssistant::abandonTurn(const QUuid &turnToken) {
  auto it = m_turns.find(turnToken);

  if (it == m_turns.end()) {
    return;
  }

  if (m_config.inference && !turnToken.isNull()) {
    m_config.inference->abortChatRequest(turnToken);
  }

  const QString replyNode = it.value().replyNode;

  m_turnUserText.remove(turnToken);
  m_nodeToTurn.remove(replyNode);
  m_turns.erase(it);

  emit assistantTurnFinished(replyNode);

  if (m_turns.isEmpty()) {
    emit statusChanged(tr("Idle"));
  }
}

void LoreAssistant::onTurnWatchdog() {
  if (m_turns.isEmpty()) {
    return;
  }

  const QList<QUuid> tokens = m_turns.keys();

  for (const QUuid &token : tokens) {
    auto it = m_turns.find(token);

    if (it == m_turns.end()) {
      continue;
    }

    if (m_config.inference && !m_config.inference->isRequestActive(token)) {
      emit statusMessage(tr("The model stopped responding."));
      abandonTurn(token);
    }
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
    return;
  }

  it->state = ChatNode::State::Done;
  it->result = line;
  it->updatedAt = QDateTime::currentDateTime();

  emit jobCompleted(requestId, line);
}