#include "../../include/ingest/NoteWriter.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTextStream>

namespace {

QString yamlEscape(const QString &value) {
  QString escaped = value;
  escaped.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
  escaped.replace(QLatin1Char('"'), QStringLiteral("\\\""));
  return escaped;
}

QString provenanceBlock(const ExtractedDocument &document) {
  const ExtractedMetadata &m = document.metadata;
  QString out;
  QTextStream stream(&out);
  stream << "---\n";
  stream << "source: \"" << yamlEscape(m.sourceName) << "\"\n";
  stream << "source-path: \"" << yamlEscape(m.sourcePath) << "\"\n";
  if (!m.sourceHash.isEmpty()) {
    stream << "source-hash: \"" << yamlEscape(m.sourceHash) << "\"\n";
  }
  stream << "extracted: \""
         << m.extractedAt.toUTC().toString(Qt::ISODate) << "\"\n";
  if (m.pageCount > 0) {
    stream << "pages: " << m.pageCount << "\n";
  }
  if (!m.mimeType.isEmpty()) {
    stream << "mime: \"" << yamlEscape(m.mimeType) << "\"\n";
  }
  stream << "---\n";
  return out;
}

QString headingPrefix(int level) {
  const int clamped = qBound(1, level, 6);
  return QString(clamped, QLatin1Char('#'));
}

// Splits "name.md" into ("name", ".md"). If there is no extension, the
// suffix is empty and the whole string is the stem.
void splitExtension(const QString &fileName, QString &stem,
                    QString &suffix) {
  const int dot = fileName.lastIndexOf(QLatin1Char('.'));
  if (dot <= 0) {
    stem = fileName;
    suffix.clear();
    return;
  }
  stem = fileName.left(dot);
  suffix = fileName.mid(dot);
}

} // namespace

QString NoteWriter::compose(const ExtractedDocument &document,
                            const IngestOptions &options) {
  QString out;
  QTextStream stream(&out);

  if (options.writeProvenance) {
    stream << provenanceBlock(document) << "\n";
  }

  const QString title =
      document.title.isEmpty() ? QStringLiteral("Untitled") : document.title;
  stream << "# " << title << "\n\n";

  for (const ExtractedSection &section : document.sections) {
    if (!section.heading.isEmpty()) {
      stream << headingPrefix(section.level) << " " << section.heading
             << "\n\n";
    }
    if (!section.body.isEmpty()) {
      stream << section.body;
      if (!section.body.endsWith(QLatin1Char('\n'))) {
        stream << "\n";
      }
      stream << "\n";
    }
  }

  return out;
}

QString NoteWriter::deriveFileName(const ExtractedDocument &document,
                                   const IngestOptions &options) {
  QString base = options.noteNameOverride;
  if (base.isEmpty()) {
    base = document.title;
  }
  if (base.isEmpty()) {
    base = QFileInfo(document.metadata.sourceName).completeBaseName();
  }
  if (base.isEmpty()) {
    base = QStringLiteral("Imported");
  }

  // Strip anything that cannot appear in a file name on the platforms we
  // target, collapse whitespace, and trim.
  static const QRegularExpression illegal(
      QStringLiteral(R"([/\\:*?"<>|\x00-\x1F])"));
  base.remove(illegal);
  base = base.simplified();
  if (base.isEmpty()) {
    base = QStringLiteral("Imported");
  }
  if (!base.endsWith(QStringLiteral(".md"), Qt::CaseInsensitive)) {
    base += QStringLiteral(".md");
  }
  return base;
}

QString NoteWriter::deriveUniqueFileName(
    const ExtractedDocument &document, const IngestOptions &options,
    const QString &destinationFolder) {
  const QString base = deriveFileName(document, options);

  if (destinationFolder.isEmpty()) {
    return base;
  }

  const QDir dir(destinationFolder);
  if (!dir.exists()) {
    return base;
  }

  if (!QFileInfo::exists(dir.filePath(base))) {
    return base;
  }

  QString stem;
  QString suffix;
  splitExtension(base, stem, suffix);

  for (int counter = 2; counter < 10000; ++counter) {
    const QString candidate =
        QStringLiteral("%1 (%2)%3").arg(stem).arg(counter).arg(suffix);
    if (!QFileInfo::exists(dir.filePath(candidate))) {
      return candidate;
    }
  }

  return base;
}

NoteWriter::Result DiskNoteWriter::write(const ExtractedDocument &document,
                                         const IngestOptions &options,
                                         const QString &absolutePath,
                                         bool overwrite) {
  Result result;

  if (absolutePath.isEmpty()) {
    result.error = QStringLiteral("No destination path given.");
    return result;
  }

  if (!overwrite && QFile::exists(absolutePath)) {
    result.error =
        QStringLiteral("A file already exists at: %1").arg(absolutePath);
    return result;
  }

  const QFileInfo info(absolutePath);
  const QDir parent = info.absoluteDir();
  if (!parent.exists() && !QDir().mkpath(parent.absolutePath())) {
    result.error =
        QStringLiteral("Cannot create folder: %1").arg(parent.absolutePath());
    return result;
  }

  QSaveFile file(absolutePath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    result.error = QStringLiteral("Cannot open for writing: %1")
                       .arg(file.errorString());
    return result;
  }

  const QString markdown = compose(document, options);
  const QByteArray bytes = markdown.toUtf8();

  if (file.write(bytes) != bytes.size()) {
    file.cancelWriting();
    result.error =
        QStringLiteral("Short write to: %1").arg(absolutePath);
    return result;
  }

  if (!file.commit()) {
    result.error = QStringLiteral("Cannot commit write: %1")
                       .arg(file.errorString());
    return result;
  }

  result.path = info.absoluteFilePath();
  return result;
}