#include "../../include/ingest/Extractor.h"
#include "../../include/ingest/ZipReader.h"

#include <QCryptographicHash>
#include <QFile>
#include <QFileInfo>
#include <QTextDocument>
#include <QXmlStreamReader>

namespace {

// EPUB is a ZIP with META-INF/container.xml pointing at an OPF file.
// The OPF spine lists XHTML chapters in reading order. One section per
// chapter.
class EpubExtractor final : public Extractor {
public:
  QString name() const override { return QStringLiteral("EPUB"); }
  QStringList extensions() const override {
    return {QStringLiteral("epub")};
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

    if (!zip->contains(QStringLiteral("META-INF/container.xml"))) {
      finishError(token, QStringLiteral("Not an EPUB: missing META-INF/container.xml"),
                  done);
      return token;
    }

    if (progress) progress(0.3, QStringLiteral("Locating OPF"));

    QString opfPath = findOpfPath(*zip);
    if (opfPath.isEmpty()) {
      finishError(token, QStringLiteral("EPUB has no OPF file."), done);
      return token;
    }

    if (progress) progress(0.5, QStringLiteral("Reading spine"));

    const QList<QString> chapters = readSpine(*zip, opfPath);
    if (chapters.isEmpty()) {
      finishError(token, QStringLiteral("EPUB spine is empty."), done);
      return token;
    }

    ExtractedDocument result;
    result.title = QFileInfo(path).completeBaseName();
    result.metadata.sourcePath = QFileInfo(path).absoluteFilePath();
    result.metadata.sourceName = QFileInfo(path).fileName();
    result.metadata.sourceHash = hashFile(path);
    result.metadata.extractedAt = QDateTime::currentDateTimeUtc();
    result.metadata.pageCount = chapters.size();
    result.metadata.mimeType = QStringLiteral("application/epub+zip");

    for (int i = 0; i < chapters.size(); ++i) {
      if (m_cancelled.value(token)) {
        m_cancelled.remove(token);
        return token;
      }

      QString readError;
      const QByteArray xhtml = zip->read(chapters.at(i), readError);
      if (!readError.isEmpty()) continue;

      QTextDocument doc;
      doc.setHtml(QString::fromUtf8(xhtml));

      ExtractedSection section;
      section.level = 2;
      section.heading = QStringLiteral("Chapter %1").arg(i + 1);
      section.body = doc.toPlainText().trimmed();
      result.sections.append(section);

      if (progress) {
        progress(0.5 + 0.45 * (static_cast<double>(i + 1) / chapters.size()),
                 QStringLiteral("Chapter %1 of %2")
                     .arg(i + 1)
                     .arg(chapters.size()));
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
  static QString findOpfPath(const ZipReader &zip) {
    QString error;
    const QByteArray container =
        zip.read(QStringLiteral("META-INF/container.xml"), error);
    if (!error.isEmpty()) return {};

    QXmlStreamReader reader(container);
    while (!reader.atEnd()) {
      reader.readNext();
      if (reader.isStartElement() &&
          reader.name() == QStringLiteral("rootfile")) {
        return reader.attributes()
            .value(QStringLiteral("full-path"))
            .toString();
      }
    }
    return {};
  }

  static QList<QString> readSpine(const ZipReader &zip,
                                  const QString &opfPath) {
    QString error;
    const QByteArray opf = zip.read(opfPath, error);
    if (!error.isEmpty()) return {};

    // Manifest maps id -> href.
    QHash<QString, QString> manifest;
    QList<QString> spineIds;

    const QString baseDir = QFileInfo(opfPath).path();

    QXmlStreamReader reader(opf);
    while (!reader.atEnd()) {
      reader.readNext();
      if (reader.isStartElement()) {
        if (reader.name() == QStringLiteral("item")) {
          const QString id =
              reader.attributes().value(QStringLiteral("id")).toString();
          const QString href =
              reader.attributes().value(QStringLiteral("href")).toString();
          if (!id.isEmpty() && !href.isEmpty()) {
            manifest.insert(id, href);
          }
        } else if (reader.name() == QStringLiteral("itemref")) {
          const QString idref =
              reader.attributes().value(QStringLiteral("idref")).toString();
          if (!idref.isEmpty()) {
            spineIds.append(idref);
          }
        }
      }
    }

    QList<QString> chapters;
    for (const QString &id : spineIds) {
      QString href = manifest.value(id);
      if (href.isEmpty()) continue;
      if (!baseDir.isEmpty() && !href.startsWith(QLatin1Char('/'))) {
        href = baseDir + QLatin1Char('/') + href;
      }
      chapters.append(href);
    }
    return chapters;
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

std::unique_ptr<Extractor> makeEpubExtractor() {
  return std::make_unique<EpubExtractor>();
}