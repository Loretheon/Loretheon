#include "../../include/ingest/IngestService.h"
#include "../../include/ingest/NoteWriter.h"

#include <QDir>
#include <QFileInfo>
#include <QMetaObject>
#include <QRunnable>
#include <QThreadPool>

struct IngestService::Pending {
  quint64 token = 0;
  QString sourcePath;
  IngestOptions options;
  Callback done;
  Extractor *extractor = nullptr;
  quint64 extractorToken = 0;
  bool slotHeld = false;
};

namespace {

class ExtractTask : public QRunnable {
public:
  ExtractTask(Extractor *extractor, const QString &path,
              const IngestOptions &options, Extractor::Callback done,
              Extractor::ProgressCallback progress)
      : m_extractor(extractor), m_path(path), m_options(options),
        m_done(std::move(done)), m_progress(std::move(progress)) {}

  void run() override {
    m_extractor->extract(m_path, m_options, std::move(m_done),
                         std::move(m_progress));
  }

private:
  Extractor *m_extractor;
  QString m_path;
  IngestOptions m_options;
  Extractor::Callback m_done;
  Extractor::ProgressCallback m_progress;
};

} // namespace

IngestService::IngestService(IngestRegistry *registry, NoteWriter *writer,
                             QObject *parent)
    : QObject(parent), m_registry(registry), m_writer(writer),
      m_slots(4) {}

IngestService::~IngestService() = default;

quint64 IngestService::import(const QString &sourcePath,
                              const IngestOptions &options, Callback done,
                              ProgressCallback progress) {
  if (!m_registry) {
    Outcome outcome;
    outcome.error = QStringLiteral("No ingest registry available.");
    if (done) done(std::move(outcome));
    return 0;
  }

  Extractor *extractor = m_registry->forPath(sourcePath);
  if (!extractor) {
    Outcome outcome;
    outcome.error = QStringLiteral("No extractor for: %1").arg(sourcePath);
    if (done) done(std::move(outcome));
    return 0;
  }

  if (!m_slots.tryAcquire()) {
    Outcome outcome;
    outcome.error =
        QStringLiteral("Import queue is full. Try again in a moment.");
    if (done) done(std::move(outcome));
    return 0;
  }

  const quint64 token = m_nextToken++;

  auto pending = std::make_shared<Pending>();
  pending->token = token;
  pending->sourcePath = sourcePath;
  pending->options = options;
  pending->done = std::move(done);
  pending->extractor = extractor;
  pending->slotHeld = true;
  m_pending.insert(token, pending);

  auto *task = new ExtractTask(
      extractor, sourcePath, options,
      [this, token](Extractor::Result result) {
        QMetaObject::invokeMethod(
            this,
            [this, token, result = std::move(result)]() {
              auto it = m_pending.find(token);
              if (it == m_pending.end()) {
                // Cancelled before completion.
                return;
              }
              auto pending = it.value();

              if (!result.ok()) {
                Outcome outcome;
                outcome.error = result.error;
                finish(token, outcome);
                return;
              }

              const QString fileName = NoteWriter::deriveUniqueFileName(
                  result.document, pending->options,
                  pending->options.destinationFolder);

              const QString notePath =
                  pending->options.destinationFolder + QLatin1Char('/') +
                  fileName;

              NoteWriter::Result writeResult =
                  m_writer->write(result.document, pending->options, notePath,
                                  /*overwrite=*/false);

              if (!writeResult.ok()) {
                Outcome outcome;
                outcome.error = writeResult.error;
                finish(token, outcome);
                return;
              }

              Outcome outcome;
              outcome.notePath = writeResult.path;
              finish(token, outcome);
            },
            Qt::QueuedConnection);
      },
      std::move(progress));

  pending->extractorToken = token;
  task->setAutoDelete(true);
  QThreadPool::globalInstance()->start(task);

  return token;
}

void IngestService::cancel(quint64 token) {
  auto it = m_pending.find(token);
  if (it == m_pending.end()) {
    return;
  }

  auto pending = it.value();
  m_pending.erase(it);

  if (pending->extractor) {
    pending->extractor->cancel(pending->extractorToken);
  }

  if (pending->slotHeld) {
    pending->slotHeld = false;
    m_slots.release();
  }
}

bool IngestService::canImport(const QString &path) const {
  return m_registry && m_registry->canHandle(path);
}

QStringList IngestService::importableExtensions() const {
  return m_registry ? m_registry->knownExtensions() : QStringList{};
}

void IngestService::setMaxConcurrent(int max) {
  const int clamped = qMax(1, max);
  const int delta = clamped - m_maxConcurrent;
  m_maxConcurrent = clamped;
  if (delta > 0) {
    m_slots.release(delta);
  } else if (delta < 0) {
    // Reduce available slots. Any in-flight imports will release on
    // completion, so the effective ceiling drops after they finish.
    m_slots.tryAcquire(-delta);
  }
}

int IngestService::maxConcurrent() const { return m_maxConcurrent; }

int IngestService::activeCount() const { return m_pending.size(); }

void IngestService::finish(quint64 token, Outcome outcome) {
  auto it = m_pending.find(token);
  if (it == m_pending.end()) {
    return;
  }
  auto pending = it.value();
  m_pending.erase(it);

  if (pending->slotHeld) {
    pending->slotHeld = false;
    m_slots.release();
  }

  if (pending->done) {
    pending->done(std::move(outcome));
  }
}