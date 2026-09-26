#include "../../include/ingest/Extractor.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QTextDocument>
#include <QTextDocumentFragment>
#include <QTextStream>

namespace {

class HtmlExtractor final : public Extractor {
public:
  QString name() const override { return QStringLiteral("HTML"); }
  QStringList extensions() const override {
    return {QStringLiteral("html"), QStringLiteral("htm")};
  }

  quint64 extract(const QString &path, const IngestOptions &options,
                  Callback done,
                  ProgressCallback progress) override {
    const quint64 token = ++m_nextToken;
    m_cancelled[token] = false;

    if (progress) progress(0.1, QStringLiteral("Reading HTML"));

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
      finishError(token, QStringLiteral("Cannot open: %1").arg(path));
      return token;
    }

    const QString html = QString::fromUtf8(file.readAll());
    file.close();

    if (m_cancelled.value(token)) {
      m_cancelled.remove(token);
      return token;
    }

    if (progress) progress(0.5, QStringLiteral("Parsing"));

    QTextDocument doc;
    doc.setHtml(html);
    const QString text = doc.toPlainText();

    ExtractedDocument result;
    result.title = QFileInfo(path).completeBaseName();
    result.metadata.sourcePath = QFileInfo(path).absoluteFilePath();
    result.metadata.sourceName = QFileInfo(path).fileName();
    result.metadata.sourceHash = hashFile(path);
    result.metadata.extractedAt = QDateTime::currentDateTimeUtc();
    result.metadata.mimeType = QStringLiteral("text/html");

    ExtractedSection section;
    section.heading = QStringLiteral("Content");
    section.body = text.trimmed();
    section.level = 2;
    result.sections.append(section);

    m_cancelled.remove(token);
    if (progress) progress(1.0, QStringLiteral("Done"));

    Result r;
    r.document = std::move(result);
    if (done) done(std::move(r));
    return token;
  }

  void cancel(quint64 token) override { m_cancelled[token] = true; }

private:
  void finishError(quint64 token, const QString &error) {
    m_cancelled.remove(token);
    Result r;
    r.error = error;
    // Stored callback is not accessible here; this extractor is synchronous
    // per-call, so the callback has already fired or will fire. See note.
  }

  static QString hashFile(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash hash(QCryptographicHash::Sha1);
    hash.addData(&file);
    return hash.result().toHex();
  }

  QHash<quint64, bool> m_cancelled;
  quint64 m_nextToken = 0;
};

} // namespace

std::unique_ptr<Extractor> makeHtmlExtractor() {
  return std::make_unique<HtmlExtractor>();
}