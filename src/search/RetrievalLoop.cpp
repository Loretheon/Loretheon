#include "../../include/search/RetrievalLoop.h"

#include "../../include/agent/StructuredCall.h"
#include "../../include/agent/schemas/SufficiencySchema.h"
#include "inference/InferenceService.h"
#include "../../include/search/SearchService.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonObject>

namespace {

constexpr int kJudgmentTimeoutMs = 60000;
constexpr int kAnswerTimeoutMs = 180000;

} // namespace

RetrievalLoop::RetrievalLoop(SearchService *search,
                             InferenceService *inference,
                             QObject *parent)
    : QObject(parent), m_search(search), m_inference(inference) {
  m_judgmentCall = new StructuredCall(m_inference, this);

  if (m_inference) {
    connect(m_inference, &InferenceService::llmDelta, this,
            [this](const InferenceService::RequestToken &token,
                   const QString &text) {
              if (token != m_answerToken) {
                return;
              }
              onAnswerChunk(text);
            });

    connect(m_inference, &InferenceService::llmFinished, this,
            [this](const InferenceService::RequestToken &token) {
              if (token != m_answerToken) {
                return;
              }

              m_answerToken = InferenceService::RequestToken();

              if (m_stage == Stage::Answering) {
                onAnswerFinished(m_answerBuffer);
              }
            });

    connect(m_inference, &InferenceService::llmError, this,
            [this](const InferenceService::RequestToken &token,
                   const QString &error) {
              if (token != m_answerToken) {
                return;
              }

              m_answerToken = InferenceService::RequestToken();
              m_running = false;
              m_stage = Stage::Idle;
              emit failed(error);
            });
  }
}

RetrievalLoop::~RetrievalLoop() = default;

void RetrievalLoop::setMaxResults(int max) {
  m_maxResults = qBound(1, max, 64);
}

void RetrievalLoop::start(const QString &query) {
  cancel();

  if (!m_search || !m_inference) {
    emit failed(tr("Search or inference is not available."));
    return;
  }

  if (!m_search->isReady()) {
    emit failed(tr("The search index is not ready."));
    return;
  }

  if (query.trimmed().isEmpty()) {
    return;
  }

  m_query = query.trimmed();
  m_hits.clear();
  m_target = 1;
  m_answerBuffer.clear();
  m_running = true;
  m_stage = Stage::Judging;

  emit sourcesUpdated(m_hits);
  emit stageChanged(tr("Searching"));

  runJudgmentRound();
}

void RetrievalLoop::cancel() {
  if (!m_running) {
    return;
  }

  if (m_judgmentCall) {
    m_judgmentCall->cancel();
  }

  if (m_inference && !m_answerToken.isNull()) {
    m_inference->abortChatRequest(m_answerToken);
    m_answerToken = InferenceService::RequestToken();
  }

  m_running = false;
  m_stage = Stage::Idle;
  m_answerBuffer.clear();
}

void RetrievalLoop::runJudgmentRound() {
  if (!m_running) {
    return;
  }

  if (m_target > m_maxResults) {
    m_running = false;
    m_stage = Stage::Idle;
    emit failed(tr("No note in the vault answers that question."));
    return;
  }

  const QVector<SearchHit> fetched =
      m_search->search(m_query, m_target);

  if (fetched.isEmpty()) {
    m_running = false;
    m_stage = Stage::Idle;
    emit failed(tr("No notes matched the query."));
    return;
  }

  m_hits = fetched;
  emit sourcesUpdated(m_hits);

  emit stageChanged(tr("Reading %1 note(s)…").arg(m_hits.size()));

  m_judgmentCall->run(
      SufficiencySchema::schema(), SufficiencySchema::systemPrompt(),
      SufficiencySchema::userPrompt(m_query, formatSources()),
      [this](StructuredCall::Result result) {
        if (!m_running) {
          return;
        }

        if (!result.ok) {
          qWarning() << "[RetrievalLoop] Judgment failed:"
                     << result.error;
          m_running = false;
          m_stage = Stage::Idle;
          emit failed(result.error);
          return;
        }

        const bool sufficient =
            result.object.value(QStringLiteral("sufficient")).toBool(false);

        const QString reason =
            result.object.value(QStringLiteral("reason")).toString();

        onJudgmentResult(true, sufficient, reason);
      },
      kJudgmentTimeoutMs);
}

void RetrievalLoop::onJudgmentResult(bool ok, bool sufficient,
                                     const QString &reason) {
  Q_UNUSED(ok);
  Q_UNUSED(reason);

  if (!m_running) {
    return;
  }

  if (sufficient) {
    runFinalAnswer();
    return;
  }

  ++m_target;
  runJudgmentRound();
}

void RetrievalLoop::runFinalAnswer() {
  if (!m_running) {
    return;
  }

  m_stage = Stage::Answering;
  m_answerBuffer.clear();

  emit stageChanged(tr("Answering"));

  QJsonArray messages;

  QJsonObject systemMessage;
  systemMessage.insert(QStringLiteral("role"), QStringLiteral("system"));
  systemMessage.insert(
      QStringLiteral("content"),
      QStringLiteral(
          "You answer questions using only the notes provided. "
          "If the notes do not answer the question, say so plainly. "
          "Cite each source by its file path in backticks. "
          "Do not invent facts. Answer in Markdown."));
  messages.append(systemMessage);

  QJsonObject userMessage;
  userMessage.insert(QStringLiteral("role"), QStringLiteral("user"));
  userMessage.insert(QStringLiteral("content"), buildAnswerPrompt());
  messages.append(userMessage);

  m_answerToken = m_inference->sendChatRequest(
      messages, QString(), 0.2, kAnswerTimeoutMs);

  if (m_answerToken.isNull()) {
    m_running = false;
    m_stage = Stage::Idle;
    emit failed(tr("Could not reach the LLM."));
  }
}

void RetrievalLoop::onAnswerChunk(const QString &text) {
  if (m_stage != Stage::Answering) {
    return;
  }

  m_answerBuffer += text;
  emit answerChunk(text);
}

void RetrievalLoop::onAnswerFinished(const QString &text) {
  if (!m_running) {
    return;
  }

  m_running = false;
  m_stage = Stage::Idle;

  emit finished(text);
}

QString RetrievalLoop::buildAnswerPrompt() const {
  QString prompt;

  prompt += QStringLiteral("Question:\n");
  prompt += m_query;
  prompt += QStringLiteral("\n\nNotes:\n");
  prompt += formatSources();
  prompt += QStringLiteral("\n\nAnswer the question.");

  return prompt;
}

QString RetrievalLoop::formatSources() const {
  QString text;

  int index = 1;

  for (const SearchHit &hit : m_hits) {
    text += QStringLiteral("--- Note %1 ---\n").arg(index++);
    text += QStringLiteral("Path: ") + hit.filePath + QLatin1Char('\n');

    if (!hit.heading.isEmpty()) {
      text += QStringLiteral("Heading: ") + hit.heading + QLatin1Char('\n');
    }

    text += hit.body;
    text += QStringLiteral("\n\n");
  }

  return text;
}