#include "../../include/ingest/Extractor.h"
#include "../../include/ingest/ZipReader.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QStringView>
#include <QXmlStreamReader>

namespace {

// Extracts text from DOCX word/document.xml. Heading styles (Heading1,
// Heading2, ...) become section boundaries.
class DocxExtractor final : public Extractor {
public:
  QString name() const override { return QStringLiteral("DOCX"); }
  QStringList extensions() const override {
    return {QStringLiteral("docx")};
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

    if (!zip->contains(QStringLiteral("word/document.xml"))) {
      finishError(token,
                  QStringLiteral("Not a DOCX: missing word/document.xml"),
                  done);
      return token;
    }

    if (progress) progress(0.4, QStringLiteral("Reading document"));

    QString xmlError;
    const QByteArray xml =
        zip->read(QStringLiteral("word/document.xml"), xmlError);
    if (!xmlError.isEmpty()) {
      finishError(token, xmlError, done);
      return token;
    }

    if (m_cancelled.value(token)) {
      m_cancelled.remove(token);
      return token;
    }

    if (progress) progress(0.7, QStringLiteral("Parsing"));

    ExtractedDocument result = parseXml(xml);
    result.title = QFileInfo(path).completeBaseName();
    result.metadata.sourcePath = QFileInfo(path).absoluteFilePath();
    result.metadata.sourceName = QFileInfo(path).fileName();
    result.metadata.sourceHash = hashFile(path);
    result.metadata.extractedAt = QDateTime::currentDateTimeUtc();
    result.metadata.mimeType = QStringLiteral(
        "application/vnd.openxmlformats-officedocument.wordprocessingml."
        "document");

    m_cancelled.remove(token);
    if (progress) progress(1.0, QStringLiteral("Done"));

    Result r;
    r.document = std::move(result);
    if (done) done(std::move(r));
    return token;
  }

  void cancel(quint64 token) override { m_cancelled[token] = true; }

private:
  ExtractedDocument parseXml(const QByteArray &xml) {
    ExtractedDocument doc;
    QXmlStreamReader reader(xml);

    ExtractedSection current;
    current.level = 2;
    QString currentText;
    bool inText = false;
    QString currentStyle;

    while (!reader.atEnd()) {
      reader.readNext();

      if (reader.isStartElement()) {
        const QStringView name = reader.name();

        if (name == QStringLiteral("pStyle")) {
          currentStyle = reader.attributes()
                             .value(QStringLiteral("w:val"))
                             .toString();
        } else if (name == QStringLiteral("t")) {
          inText = true;
        } else if (name == QStringLiteral("p")) {
          currentText.clear();
          currentStyle.clear();
        }
      } else if (reader.isCharacters() && inText) {
        currentText.append(reader.text().toString());
      } else if (reader.isEndElement()) {
        const QStringView name = reader.name();

        if (name == QStringLiteral("t")) {
          inText = false;
        } else if (name == QStringLiteral("p")) {
          if (currentText.trimmed().isEmpty()) {
            continue;
          }

          if (currentStyle.startsWith(QStringLiteral("Heading"))) {
            bool ok = false;
            const int level = currentStyle.mid(7).toInt(&ok);
            if (ok && level > 0) {
              if (!current.heading.isEmpty() || !current.body.isEmpty()) {
                doc.sections.append(current);
                current = ExtractedSection{};
                current.level = 2;
              }
              current.heading = currentText.trimmed();
              current.level = qBound(2, level + 1, 6);
              currentText.clear();
              continue;
            }
          }

          if (!current.body.isEmpty()) {
            current.body += QStringLiteral("\n\n");
          }
          current.body += currentText.trimmed();
          currentText.clear();
        }
      }
    }

    if (!current.heading.isEmpty() || !current.body.isEmpty()) {
      doc.sections.append(current);
    }

    return doc;
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

std::unique_ptr<Extractor> makeDocxExtractor() {
  return std::make_unique<DocxExtractor>();
}