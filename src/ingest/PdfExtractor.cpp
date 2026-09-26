#include "../../include/ingest/Extractor.h"

#include <QPdfDocument>
#include <QPdfSelection>

#include <QEventLoop>
#include <QFile>
#include <QFileInfo>
#include <QCryptographicHash>

namespace {

class PdfExtractor final : public Extractor {
public:
  QString name() const override { return QStringLiteral("PDF"); }

  QStringList extensions() const override { return {QStringLiteral("pdf")}; }

  quint64 extract(const QString &path, const IngestOptions &options,
                  Callback done,
                  ProgressCallback progress) override {
    const quint64 token = ++m_nextToken;
    auto state = std::make_shared<State>();
    state->token = token;
    state->done = std::move(done);
    state->progress = std::move(progress);
    m_states[token] = state;

    auto *doc = new QPdfDocument();

    QObject::connect(doc, &QPdfDocument::statusChanged, doc,
                     [this, doc, state, path, options](QPdfDocument::Status s) {
                       if (s != QPdfDocument::Status::Ready &&
                           s != QPdfDocument::Status::Error) {
                         return;
                       }
                       if (state->cancelled) {
                         cleanup(state->token, doc);
                         return;
                       }
                       if (s == QPdfDocument::Status::Error) {
                         finishWithError(state->token,
                                         QStringLiteral("Failed to load PDF: %1")
                                             .arg(path));
                         doc->deleteLater();
                         return;
                       }
                       runExtraction(doc, path, options, state);
                       doc->deleteLater();
                     });

    doc->load(path);
    return token;
  }

  void cancel(quint64 token) override {
    auto it = m_states.find(token);
    if (it != m_states.end()) {
      it.value()->cancelled = true;
    }
  }

private:
  struct State {
    quint64 token = 0;
    Callback done;
    ProgressCallback progress;
    bool cancelled = false;
  };

  void runExtraction(QPdfDocument *doc, const QString &path,
                     const IngestOptions &options,
                     std::shared_ptr<State> state) {
    if (!doc || doc->pageCount() == 0) {
      finishWithError(state->token, QStringLiteral("PDF has no pages."));
      return;
    }

    if (state->progress) {
      state->progress(0.1, QStringLiteral("Extracting text"));
    }

    ExtractedDocument result;
    result.title = QFileInfo(path).completeBaseName();
    result.metadata.sourcePath = QFileInfo(path).absoluteFilePath();
    result.metadata.sourceName = QFileInfo(path).fileName();
    result.metadata.sourceHash = hashFile(path);
    result.metadata.extractedAt = QDateTime::currentDateTimeUtc();
    result.metadata.pageCount = doc->pageCount();
    result.metadata.mimeType = QStringLiteral("application/pdf");

    const int pageCount = doc->pageCount();
    for (int page = 0; page < pageCount; ++page) {
      if (state->cancelled) {
        cleanup(state->token, nullptr);
        return;
      }

      const QPdfSelection selection = doc->getAllText(page);
      ExtractedSection section;
      section.heading = QStringLiteral("Page %1").arg(page + 1);
      section.body = selection.text().trimmed();
      section.level = 2;
      result.sections.append(section);

      if (state->progress) {
        const double frac =
            0.1 + 0.85 * (static_cast<double>(page + 1) / pageCount);
        state->progress(frac, QStringLiteral("Page %1 of %2")
                                 .arg(page + 1)
                                 .arg(pageCount));
      }
    }

    if (state->progress) {
      state->progress(1.0, QStringLiteral("Done"));
    }

    finishWithSuccess(state->token, std::move(result));
  }

  void finishWithSuccess(quint64 token, ExtractedDocument doc) {
    auto it = m_states.find(token);
    if (it == m_states.end()) return;
    auto state = it.value();
    m_states.erase(it);
    if (state->done) {
      Result r;
      r.document = std::move(doc);
      state->done(std::move(r));
    }
  }

  void finishWithError(quint64 token, const QString &error) {
    auto it = m_states.find(token);
    if (it == m_states.end()) return;
    auto state = it.value();
    m_states.erase(it);
    if (state->done) {
      Result r;
      r.error = error;
      state->done(std::move(r));
    }
  }

  void cleanup(quint64 token, QObject *doc) {
    auto it = m_states.find(token);
    if (it != m_states.end()) {
      m_states.erase(it);
    }
    if (doc) doc->deleteLater();
  }

  static QString hashFile(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash hash(QCryptographicHash::Sha1);
    if (!hash.addData(&file)) return {};
    return hash.result().toHex();
  }

  QHash<quint64, std::shared_ptr<State>> m_states;
  quint64 m_nextToken = 0;
};

} // namespace

std::unique_ptr<Extractor> makePdfExtractor() {
  return std::make_unique<PdfExtractor>();
}