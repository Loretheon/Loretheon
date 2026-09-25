#include "../../include/search/NotePromoter.h"

#include "../../include/search/ScopeIndex.h"

#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>

NotePromoter::NotePromoter(ScopeIndex *index, QObject *parent)
    : QObject(parent), m_index(index) {}

bool NotePromoter::copyMarkdownFile(const QString &source,
                                    const QString &destination,
                                    bool *skipped) {
  if (skipped)
    *skipped = false;

  const QFileInfo destinationInfo(destination);

  if (destinationInfo.exists()) {
    if (skipped)
      *skipped = true;
    return true;
  }

  const QDir parent = destinationInfo.absoluteDir();

  if (!parent.exists() && !QDir().mkpath(parent.absolutePath())) {
    return false;
  }

  QFile sourceFile(source);

  if (!sourceFile.open(QIODevice::ReadOnly)) {
    return false;
  }

  const QByteArray data = sourceFile.readAll();
  sourceFile.close();

  QFile destinationFile(destination);

  if (!destinationFile.open(QIODevice::WriteOnly)) {
    return false;
  }

  destinationFile.write(data);
  destinationFile.close();

  return true;
}

NotePromoter::Result NotePromoter::promoteFile(
    const QString &sourcePath, const QString &destinationRoot,
    const QString &sourceRoot) {
  Result result;

  const QFileInfo sourceInfo(sourcePath);

  if (!sourceInfo.exists() || !sourceInfo.isFile()) {
    result.failed.append(sourcePath);
    return result;
  }

  if (sourceInfo.suffix().compare(QStringLiteral("md"),
                                  Qt::CaseInsensitive) != 0) {
    return result;
  }

  const QString relative =
      QDir(sourceRoot).relativeFilePath(sourceInfo.absoluteFilePath());

  const QString destination =
      QDir(destinationRoot).filePath(relative);

  bool skipped = false;

  if (!copyMarkdownFile(sourceInfo.absoluteFilePath(), destination,
                        &skipped)) {
    result.failed.append(sourceInfo.absoluteFilePath());
    return result;
  }

  if (skipped) {
    result.skipped.append(destination);
    return result;
  }

  if (m_index) {
    const int added = m_index->addFile(destination);

    if (added < 0) {
      result.copiedNotIndexed.append(destination);
      return result;
    }
  }

  result.written.append(destination);
  return result;
}

NotePromoter::Result NotePromoter::promote(const QString &sourcePath,
                                           const QString &sessionName,
                                           const QString &notesRoot) {
  Result total;

  if (sourcePath.isEmpty()) {
    total.error = QStringLiteral("No source path given.");
    return total;
  }

  if (sessionName.isEmpty()) {
    total.error = QStringLiteral("No session name given.");
    return total;
  }

  if (notesRoot.isEmpty()) {
    total.error = QStringLiteral("No notes root given.");
    return total;
  }

  const QFileInfo sourceInfo(sourcePath);

  if (!sourceInfo.exists()) {
    total.error =
        QStringLiteral("Source does not exist: %1").arg(sourcePath);
    return total;
  }

  const QString destinationRoot =
      QDir(notesRoot).filePath(sessionName);

  if (sourceInfo.isFile()) {
    const QString sourceRoot = sourceInfo.absolutePath();
    const Result one =
        promoteFile(sourcePath, destinationRoot, sourceRoot);

    total.written += one.written;
    total.skipped += one.skipped;
    total.copiedNotIndexed += one.copiedNotIndexed;
    total.failed += one.failed;
    return total;
  }

  if (!sourceInfo.isDir()) {
    total.error = QStringLiteral("Source is not a file or folder.");
    return total;
  }

  const QString sourceRoot = sourceInfo.absoluteFilePath();

  QStringList files;

  QDirIterator it(sourceRoot, QStringList{QStringLiteral("*.md")},
                  QDir::Files | QDir::Readable,
                  QDirIterator::Subdirectories);

  while (it.hasNext()) {
    files.append(it.next());
  }

  files.sort(Qt::CaseInsensitive);

  const int totalFiles = files.size();
  int processed = 0;

  for (const QString &file : std::as_const(files)) {
    const Result one = promoteFile(file, destinationRoot, sourceRoot);

    total.written += one.written;
    total.skipped += one.skipped;
    total.copiedNotIndexed += one.copiedNotIndexed;
    total.failed += one.failed;

    ++processed;
    emit progress(processed, totalFiles);
  }

  return total;
}