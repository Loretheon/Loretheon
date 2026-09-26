#pragma once

#include "StructuredSchema.h"
#include "inference/InferenceService.h"

#include <QJsonObject>
#include <QObject>
#include <QString>

#include <functional>

class InferenceService;

// One-shot structured request: build the response format from a
// schema, send a two-message chat request, accumulate deltas, parse
// the JSON reply, hand it to the callback.
//
// The call owns itself until the LLM responds or fails. It is safe to
// create one per request; there is no shared state.
//
// The callback fires exactly once, on the main thread, with either a
// parsed object or an error string.
class StructuredCall : public QObject {
  Q_OBJECT

public:
  struct Result {
    bool ok = false;
    QJsonObject object;
    QString error;
  };

  using Callback = std::function<void(Result)>;

  StructuredCall(InferenceService *inference, QObject *parent = nullptr);
  ~StructuredCall() override;

  // Run a request. systemPrompt frames the model's role; userPrompt
  // carries the actual input. schema determines the response shape.
  // timeoutMs bounds the request.
  void run(const StructuredSchema &schema,
           const QString &systemPrompt,
           const QString &userPrompt,
           Callback callback,
           int timeoutMs = 60000);

  // Cancel an in-flight request. The callback does not fire.
  void cancel();

  bool isRunning() const { return m_running; }

private:
  InferenceService *m_inference = nullptr;

  InferenceService::RequestToken m_token;
  bool m_running = false;

  QString m_accumulated;
  Callback m_callback;
  QString m_schemaName;
};