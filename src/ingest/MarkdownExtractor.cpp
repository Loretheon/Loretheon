#include "../../include/ingest/MarkdownExtractor.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QMutexLocker>

QString MarkdownExtractor::name() const {
  return QStringLiteral("Markdown");
}

QStringList MarkdownExtractor::extensions() const {
  return {QStringLiteral("md"), QStringLiteral("markdown")};
}

quint64 MarkdownExtractor::extract(const QString &path,
                                   const IngestOptions &options,
                                   Callback done,
                                   ProgressCallback progress) {
  Q_UNUSED(options);

  quint64 token = 0;

  {
    QMutexLocker locker(&m_mutex);
    token = m_nextToken++;
    m_cancelled.insert(token, false);
  }

  if (progress) {
    progress(0.0, QStringLiteral("Reading"));
  }

  QFile file(path);

  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    QMutexLocker locker(&m_mutex);
    m_cancelled.remove(token);

    Result result;
    result.error =
        QStringLiteral("Could not read the file: %1").arg(path);

    if (done) {
      done(std::move(result));
    }
    return token;
  }

  const QByteArray bytes = file.readAll();
  file.close();

  bool cancelled = false;

  {
    QMutexLocker locker(&m_mutex);
    cancelled = m_cancelled.value(token, false);
    m_cancelled.remove(token);
  }

  if (cancelled) {
    return token;
  }

  if (progress) {
    progress(1.0, QStringLiteral("Done"));
  }

  const QFileInfo info(path);

  ExtractedDocument document;
  document.title = info.completeBaseName();

  ExtractedSection section;
  section.body = QString::fromUtf8(bytes);
  section.level = 1;
  document.sections.append(section);

  document.metadata.sourcePath = info.absoluteFilePath();
  document.metadata.sourceName = info.fileName();
  document.metadata.sourceHash = QString::fromLatin1(
      QCryptographicHash::hash(bytes, QCryptographicHash::Sha1).toHex());
  document.metadata.extractedAt = QDateTime::currentDateTimeUtc();
  document.metadata.pageCount = 0;

  Result result;
  result.document = document;

  if (done) {
    done(std::move(result));
  }

  return token;
}

void MarkdownExtractor::cancel(quint64 token) {
  QMutexLocker locker(&m_mutex);
  m_cancelled.insert(token, true);
}

std::unique_ptr<Extractor> makeMarkdownExtractor() {
  return std::make_unique<MarkdownExtractor>();
}