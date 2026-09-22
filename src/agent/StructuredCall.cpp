#include "../../include/agent/StructuredCall.h"

#include "inference/InferenceService.h"

#include <QDebug>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QRegularExpression>

StructuredCall::StructuredCall(InferenceService *inference, QObject *parent)
    : QObject(parent), m_inference(inference) {
  if (!m_inference) {
    return;
  }

  connect(m_inference, &InferenceService::llmDelta, this,
          [this](const InferenceService::RequestToken &token,
                 const QString &text) {
            if (!m_running || token != m_token) {
              return;
            }
            m_accumulated += text;
          });

  connect(m_inference, &InferenceService::llmFinished, this,
          [this](const InferenceService::RequestToken &token) {
            if (!m_running || token != m_token) {
              return;
            }

            const InferenceService::RequestToken finished = m_token;
            m_token = InferenceService::RequestToken();
            m_running = false;

            QString response = m_accumulated.trimmed();
            m_accumulated.clear();

            static const QRegularExpression fenceStart(
                QStringLiteral("^\\s*```[a-zA-Z0-9_-]*\\s*\\n?"));
            static const QRegularExpression fenceEnd(
                QStringLiteral("\\n?\\s*```\\s*$"));

            response.remove(fenceStart);
            response.remove(fenceEnd);
            response = response.trimmed();

            Result result;

            QJsonParseError parseError;
            const QJsonDocument doc =
                QJsonDocument::fromJson(response.toUtf8(), &parseError);

            if (parseError.error != QJsonParseError::NoError ||
                !doc.isObject()) {
              result.ok = false;
              result.error =
                  QStringLiteral("Model reply was not valid JSON: %1")
                      .arg(parseError.errorString());
              qWarning() << "[StructuredCall] Parse failed:"
                         << parseError.errorString()
                         << "raw:" << response.left(200);
            } else {
              result.ok = true;
              result.object = doc.object();
            }

            Q_UNUSED(finished);

            if (m_callback) {
              auto callback = m_callback;
              m_callback = nullptr;
              callback(result);
            }
          });

  connect(m_inference, &InferenceService::llmError, this,
          [this](const InferenceService::RequestToken &token,
                 const QString &error) {
            if (!m_running || token != m_token) {
              return;
            }

            m_token = InferenceService::RequestToken();
            m_running = false;

            Result result;
            result.ok = false;
            result.error = error;

            if (m_callback) {
              auto callback = m_callback;
              m_callback = nullptr;
              callback(result);
            }
          });
}

StructuredCall::~StructuredCall() = default;

void StructuredCall::run(const StructuredSchema &schema,
                         const QString &systemPrompt,
                         const QString &userPrompt,
                         Callback callback,
                         int timeoutMs) {
  if (!m_inference) {
    Result result;
    result.ok = false;
    result.error = QStringLiteral("Inference service is unavailable.");
    if (callback) {
      callback(result);
    }
    return;
  }

  if (m_running) {
    cancel();
  }

  m_accumulated.clear();
  m_callback = std::move(callback);
  m_schemaName = schema.name();

  QJsonArray messages;

  QJsonObject systemMessage;
  systemMessage.insert(QStringLiteral("role"), QStringLiteral("system"));
  systemMessage.insert(QStringLiteral("content"), systemPrompt);
  messages.append(systemMessage);

  QJsonObject userMessage;
  userMessage.insert(QStringLiteral("role"), QStringLiteral("user"));
  userMessage.insert(QStringLiteral("content"), userPrompt);
  messages.append(userMessage);

  m_running = true;

  const QJsonObject responseFormat =
      schema.isEmpty() ? QJsonObject() : schema.toResponseFormat();

  m_token = m_inference->sendChatRequest(
      messages, QString(), 0.0, timeoutMs, QString(), responseFormat,
      QJsonArray(), QString());

  if (m_token.isNull()) {
    m_running = false;

    Result result;
    result.ok = false;
    result.error = QStringLiteral("Request was not dispatched.");

    if (m_callback) {
      auto cb = m_callback;
      m_callback = nullptr;
      cb(result);
    }
  }
}

void StructuredCall::cancel() {
  if (!m_running) {
    return;
  }

  if (m_inference && !m_token.isNull()) {
    m_inference->abortChatRequest(m_token);
  }

  m_token = InferenceService::RequestToken();
  m_running = false;
  m_accumulated.clear();
  m_callback = nullptr;
}