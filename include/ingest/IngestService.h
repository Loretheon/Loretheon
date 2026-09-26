#ifndef EPISTEME_INGESTSERVICE_H
#define EPISTEME_INGESTSERVICE_H

#include "ExtractedDocument.h"
#include "Extractor.h"
#include "IngestOptions.h"
#include "IngestRegistry.h"

#include <QHash>
#include <QObject>
#include <QSemaphore>
#include <QString>
#include <QStringList>

#include <functional>
#include <memory>

class NoteWriter;

// Orchestrates a single import: resolve extractor, extract on a worker
// thread, marshal the result back, write the note. The source file is
// only ever read; it is never moved, copied, or deleted.
//
// Concurrency is capped. When the cap is reached, import() fails fast
// with a "queue is full" Outcome rather than blocking or queueing. A
// caller that wants a queue should implement it above this class.
class IngestService : public QObject {
  Q_OBJECT

public:
  explicit IngestService(IngestRegistry *registry, NoteWriter *writer,
                         QObject *parent = nullptr);

  ~IngestService() override;

  struct Outcome {
    QString notePath;  // empty on failure
    QString error;     // empty on success
    bool ok() const { return error.isEmpty(); }
  };

  using Callback = std::function<void(Outcome)>;
  using ProgressCallback = Extractor::ProgressCallback;

  // Import one file. Returns a token usable with cancel(). The callback
  // fires exactly once, on the thread that owns this service (the main
  // thread), unless the import was cancelled before it completed.
  //
  // Returns 0 and calls `done` immediately with an error if the
  // concurrency cap has been reached, or if no extractor is registered
  // for the path.
  quint64 import(const QString &sourcePath, const IngestOptions &options,
                 Callback done, ProgressCallback progress = nullptr);

  // Cancel an in-flight import. The extractor is asked to stop, the
  // pending entry is dropped, and `done` will not fire. Best-effort: an
  // import that has already produced its note cannot be undone.
  void cancel(quint64 token);

  // True if any registered extractor claims this path.
  bool canImport(const QString &path) const;

  // Extensions any registered extractor claims.
  QStringList importableExtensions() const;

  // Maximum number of imports running concurrently. Default 4. Values
  // below 1 are clamped to 1.
  void setMaxConcurrent(int max);
  int maxConcurrent() const;

  // Number of imports currently in flight.
  int activeCount() const;

private:
  struct Pending;

  void finish(quint64 token, Outcome outcome);

  IngestRegistry *m_registry = nullptr;
  NoteWriter *m_writer = nullptr;

  QHash<quint64, std::shared_ptr<Pending>> m_pending;
  quint64 m_nextToken = 1;

  QSemaphore m_slots;
  int m_maxConcurrent = 4;
};

#endif // EPISTEME_INGESTSERVICE_H