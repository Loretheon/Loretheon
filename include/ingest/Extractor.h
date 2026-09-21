#ifndef EPISTEME_EXTRACTOR_H
#define EPISTEME_EXTRACTOR_H

#include "ExtractedDocument.h"
#include "IngestOptions.h"

#include <QString>
#include <QStringList>

#include <functional>
#include <memory>

// The interface every format extractor implements. Implementations must
// be stateless and re-entrant: one instance may serve many concurrent
// extractions, and all per-operation state belongs in the Callback
// closure, not in the extractor.
class Extractor {
public:
  virtual ~Extractor() = default;

  // Human-readable name, used in logs and error messages.
  virtual QString name() const = 0;

  // Lower-case extensions this extractor handles, without the dot.
  // e.g. {"pdf"} or {"docx"}.
  virtual QStringList extensions() const = 0;

  // A result is either a document or an error string. Never both.
  struct Result {
    ExtractedDocument document;
    QString error;
    bool ok() const { return error.isEmpty(); }
  };

  // Called once on completion (success or failure), on the thread that
  // owns the extractor's event loop.
  using Callback = std::function<void(Result)>;

  // Called zero or more times before completion with a value in [0, 1]
  // and an optional human-readable stage label. May be invoked from any
  // thread; implementations should document their threading.
  using ProgressCallback = std::function<void(double, const QString &)>;

  // Begin extraction. Must return promptly; work happens asynchronously
  // and completion is signalled via `done`. The returned token can be
  // passed to cancel() to abandon the operation.
  virtual quint64 extract(const QString &path, const IngestOptions &options,
                          Callback done,
                          ProgressCallback progress = nullptr) = 0;

  // Request cancellation of a previously started extraction. Best-effort:
  // a completed operation cannot be cancelled, and `done` may still fire
  // once. Implementations must guarantee that `done` fires exactly once
  // whether or not cancel() was called.
  virtual void cancel(quint64 token) = 0;
};

#endif // EPISTEME_EXTRACTOR_H