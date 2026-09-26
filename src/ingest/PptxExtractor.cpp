#include "../../include/ingest/Extractor.h"
#include "../../include/ingest/ZipReader.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QXmlStreamReader>

namespace {

// PPTX slides live at ppt/slides/slideN.xml. Each slide becomes one
// section; the slide's first text run is treated as the title.
class PptxExtractor final : public Extractor {
public:
  QString name() const override { return QStringLiteral("PPTX"); }
  QStringList extensions() const override {
    return {QStringLiteral("pptx")};
  }

  quint64 extract(const QString &path, const IngestOptions &options,
                  Callback done,
                  ProgressCallback progress) override {
    const quint64 token = ++m_nextToken;
    m_cancelled[token] = false;

    if (progress) progress(0.1, QStringLiteral("Opening archive"));

    QString zipError;
    auto zip = ZipReader::open(path, zipError);
    if (!zip) {
      finishError(token, zipError, done);
      return token;
    }

    if (!zip->contains(QStringLiteral("ppt/presentation.xml"))) {
      finishError(token, QStringLiteral("Not a PPTX: missing ppt/presentation.xml"),
                  done);
      return token;
    }

    // Collect slide XML entries in numeric order.
    QList<QString> slides;
    static const QRegularExpression slideRe(
        QStringLiteral("^ppt/slides/slide(\\d+)\\.xml$"));
    for (const QString &entry : zip->entries()) {
      const auto match = slideRe.match(entry);
      if (match.hasMatch()) {
        slides.append(entry);
      }
    }
    std::sort(slides.begin(), slides.end(),
              [](const QString &a, const QString &b) {
                const auto ra = slideRe.match(a);
                const auto rb = slideRe.match(b);
                return ra.captured(1).toInt() < rb.captured(1).toInt();
              });

    if (slides.isEmpty()) {
      finishError(token, QStringLiteral("PPTX has no slides."), done);
      return token;
    }

    ExtractedDocument result;
    result.title = QFileInfo(path).completeBaseName();
    result.metadata.sourcePath = QFileInfo(path).absoluteFilePath();
    result.metadata.sourceName = QFileInfo(path).fileName();
    result.metadata.sourceHash = hashFile(path);
    result.metadata.extractedAt = QDateTime::currentDateTimeUtc();
    result.metadata.pageCount = slides.size();
    result.metadata.mimeType = QStringLiteral(
        "application/vnd.openxmlformats-officedocument.presentationml."
        "presentation");

    for (int i = 0; i < slides.size(); ++i) {
      if (m_cancelled.value(token)) {
        m_cancelled.remove(token);
        return token;
      }

      QString xmlError;
      const QByteArray xml = zip->read(slides.at(i), xmlError);
      if (!xmlError.isEmpty()) continue;

      const QList<QString> runs = textRuns(xml);
      ExtractedSection section;
      section.level = 2;
      section.heading = QStringLiteral("Slide %1").arg(i + 1);
      section.body = runs.join(QStringLiteral("\n\n"));
      result.sections.append(section);

      if (progress) {
        progress(0.2 + 0.7 * (static_cast<double>(i + 1) / slides.size()),
                 QStringLiteral("Slide %1 of %2")
                     .arg(i + 1)
                     .arg(slides.size()));
      }
    }

    m_cancelled.remove(token);
    if (progress) progress(1.0, QStringLiteral("Done"));

    Result r;
    r.document = std::move(result);
    if (done) done(std::move(r));
    return token;
  }

  void cancel(quint64 token) override { m_cancelled[token] = true; }

private:
  static QList<QString> textRuns(const QByteArray &xml) {
    QList<QString> runs;
    QXmlStreamReader reader(xml);
    bool inText = false;
    QString current;

    while (!reader.atEnd()) {
      reader.readNext();
      if (reader.isStartElement()) {
        if (reader.name() == QStringLiteral("t")) {
          inText = true;
          current.clear();
        }
      } else if (reader.isCharacters() && inText) {
        current.append(reader.text().toString());
      } else if (reader.isEndElement()) {
        if (reader.name() == QStringLiteral("t") && inText) {
          if (!current.trimmed().isEmpty()) runs.append(current.trimmed());
          inText = false;
        }
      }
    }
    return runs;
  }

  void finishError(quint64 token, const QString &error, Callback &done) {
    m_cancelled.remove(token);
    Result r;
    r.error = error;
    if (done) done(std::move(r));
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

std::unique_ptr<Extractor> makePptxExtractor() {
  return std::make_unique<PptxExtractor>();
}