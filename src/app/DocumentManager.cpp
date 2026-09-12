#include "DocumentManager.h"

#include "Settings.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>

namespace {

constexpr QLatin1StringView MarkdownExtension("md");
constexpr QLatin1StringView TextExtension("txt");
constexpr QLatin1StringView UntitledBaseName("Untitled");

QString normalizedExtension(const QString &extension) {
  return extension.trimmed().toLower();
}

QString documentTypeName(DocumentMode type) {
  return type == DocumentMode::Markdown ? QStringLiteral("Markdown")
                                        : QStringLiteral("PlainText");
}

bool writeTextFile(const QString &path, const QString &text) {
  QSaveFile file(path);

  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    qWarning() << "[DOCUMENT] Failed to open for writing:" << path
               << "Error:" << file.errorString();
    return false;
  }

  QTextStream stream(&file);
  stream.setEncoding(QStringConverter::Utf8);
  stream << text;
  stream.flush();

  if (stream.status() != QTextStream::Ok) {
    qWarning() << "[DOCUMENT] Failed while writing:" << path
               << "Error:" << file.errorString();

    file.cancelWriting();
    return false;
  }

  if (!file.commit()) {
    qWarning() << "[DOCUMENT] Failed to commit:" << path
               << "Error:" << file.errorString();
    return false;
  }

  return true;
}

bool readTextFile(const QString &path, QString &text) {
  QFile file(path);

  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    qWarning() << "[DOCUMENT] Failed to open:" << path
               << "Error:" << file.errorString();
    return false;
  }

  QTextStream stream(&file);
  stream.setEncoding(QStringConverter::Utf8);

  text = stream.readAll();

  if (stream.status() != QTextStream::Ok) {
    qWarning() << "[DOCUMENT] Failed while reading:" << path
               << "Error:" << file.errorString();
    return false;
  }

  return true;
}

} // namespace

DocumentManager::DocumentManager(QObject *parent)
    : QObject(parent), current(nullptr) {}

TextDocument *DocumentManager::currentDocument() const { return current; }

DocumentMode DocumentManager::typeForExtension(const QString &extension) {
  return normalizedExtension(extension) == MarkdownExtension
             ? DocumentMode::Markdown
             : DocumentMode::PlainText;
}

QString DocumentManager::uniqueDefaultPath(const QString &baseName,
                                           const QString &extension) const {
  return uniquePathIn(QDir(Settings::getRootDirectory()), baseName, extension);
}

QString DocumentManager::uniquePathIn(const QDir &dir, const QString &baseName,
                                      const QString &extension) const {
  const QString normalized = normalizedExtension(extension);

  QString path =
      dir.filePath(QStringLiteral("%1.%2").arg(baseName, normalized));

  for (int suffix = 2; QFile::exists(path); ++suffix) {
    path = dir.filePath(
        QStringLiteral("%1 %2.%3").arg(baseName).arg(suffix).arg(normalized));
  }

  return path;
}

QString DocumentManager::uniqueFolderPathIn(const QDir &dir,
                                            const QString &baseName) const {
  QString path = dir.filePath(baseName);

  for (int suffix = 2; QFileInfo::exists(path); ++suffix) {
    path = dir.filePath(QStringLiteral("%1 %2").arg(baseName).arg(suffix));
  }

  return path;
}

void DocumentManager::createDocument(DocumentMode type,
                                     const QString &extension) {
  const QString path =
      uniqueDefaultPath(QString::fromLatin1(UntitledBaseName), extension);

  if (!writeTextFile(path, {})) {
    return;
  }

  auto *document = new TextDocument(this);

  document->setType(type);
  document->setFilePath(path);
  document->setModified(false);

  documents.append(document);
  current = document;

  emit documentChanged(current);
  emit documentCreated(path);
}

void DocumentManager::createDocumentIn(DocumentMode type,
                                       const QString &extension,
                                       const QString &parentPath) {
  QDir dir(parentPath);
  if (parentPath.isEmpty() || !dir.exists()) {
    createDocument(type, extension);
    return;
  }

  const QString path =
      uniquePathIn(dir, QString::fromLatin1(UntitledBaseName), extension);

  if (!writeTextFile(path, {})) {
    return;
  }

  auto *document = new TextDocument(this);

  document->setType(type);
  document->setFilePath(path);
  document->setModified(false);

  documents.append(document);
  current = document;

  emit documentChanged(current);
  emit documentCreated(path);
}

void DocumentManager::newTextFile() {
  createDocument(DocumentMode::PlainText, QString::fromLatin1(TextExtension));
}

void DocumentManager::newMarkdownFile() {
  createDocument(DocumentMode::Markdown,
                 QString::fromLatin1(MarkdownExtension));
}

void DocumentManager::newTextFileIn(const QString &parentPath) {
  createDocumentIn(DocumentMode::PlainText,
                   QString::fromLatin1(TextExtension), parentPath);
}

void DocumentManager::newMarkdownFileIn(const QString &parentPath) {
  createDocumentIn(DocumentMode::Markdown,
                   QString::fromLatin1(MarkdownExtension), parentPath);
}

void DocumentManager::newFolderIn(const QString &parentPath) {
  QDir dir(parentPath);
  if (parentPath.isEmpty() || !dir.exists()) {
    return;
  }

  const QString path =
      uniqueFolderPathIn(dir, QStringLiteral("New Folder"));

  if (!dir.mkdir(QFileInfo(path).fileName())) {
    qWarning() << "[DOCUMENT] Failed to create folder:" << path;
    return;
  }

  emit folderCreated(path);
}

bool DocumentManager::openFile(const QString &path) {
  QString text;

  if (!readTextFile(path, text)) {
    return false;
  }

  auto *document = new TextDocument(this);

  document->setPlainText(text);
  document->setType(typeForExtension(QFileInfo(path).suffix()));
  document->setFilePath(path);
  document->setModified(false);

  documents.append(document);
  current = document;

  emit documentChanged(current);
  return true;
}

bool DocumentManager::save() {
  if (!current) {
    qWarning() << "[DOCUMENT] No current document to save.";
    return false;
  }

  const QString path = current->filePath();

  if (path.isEmpty()) {
    qWarning() << "[DOCUMENT] Current document has no file path.";
    return false;
  }

  if (!writeTextFile(path, current->toPlainText())) {
    return false;
  }

  current->setModified(false);
  return true;
}

bool DocumentManager::renameFile(const QString &oldPath,
                                 const QString &newPath) {
  if (oldPath == newPath) {
    return true;
  }

  for (TextDocument *document : std::as_const(documents)) {
    if (document->filePath() != oldPath) {
      continue;
    }

    document->setFilePath(newPath);
    document->setType(typeForExtension(QFileInfo(newPath).suffix()));

    if (document == current) {
      emit documentChanged(current);
    }

    break;
  }

  emit fileRenamed(oldPath, newPath);
  return true;
}

bool DocumentManager::deleteFile(const QString &path) {
  const QFileInfo info(path);
  if (!info.exists()) {
    return false;
  }

  for (int i = documents.size() - 1; i >= 0; --i) {
    if (documents.at(i)->filePath() != path) {
      continue;
    }

    TextDocument *document = documents.takeAt(i);
    const bool wasCurrent = (document == current);
    delete document;

    if (wasCurrent) {
      current = documents.isEmpty() ? nullptr : documents.last();
      emit documentChanged(current);
    }
  }

  bool removed = false;
  if (info.isDir()) {
    removed = QDir(path).removeRecursively();
  } else {
    removed = QFile::remove(path);
  }

  if (removed) {
    emit fileDeleted(path);
  }

  return removed;
}

bool DocumentManager::convertFile(const QString &path,
                                  const QString &targetExtension,
                                  DocumentMode targetType) {
  const QFileInfo info(path);
  if (!info.exists() || info.isDir()) {
    return false;
  }

  const QDir dir = info.dir();
  const QString baseName = info.completeBaseName();
  const QString targetPath =
      uniquePathIn(dir, baseName, targetExtension);

  QString text;
  if (!readTextFile(path, text)) {
    return false;
  }

  if (!writeTextFile(targetPath, text)) {
    return false;
  }

  if (!QFile::remove(path)) {
    qWarning() << "[DOCUMENT] Converted file written but original could not "
                  "be removed:"
               << path;
  }

  for (TextDocument *document : std::as_const(documents)) {
    if (document->filePath() != path) {
      continue;
    }

    document->setFilePath(targetPath);
    document->setType(targetType);

    if (document == current) {
      emit documentChanged(current);
    }

    break;
  }

  emit fileConverted(path, targetPath);
  return true;
}

bool DocumentManager::convertToMarkdown(const QString &path) {
  return convertFile(path, QString::fromLatin1(MarkdownExtension),
                     DocumentMode::Markdown);
}

bool DocumentManager::convertToText(const QString &path) {
  return convertFile(path, QString::fromLatin1(TextExtension),
                     DocumentMode::PlainText);
}

void DocumentManager::closeCurrent() {
  if (!current) {
    return;
  }

  TextDocument *document = current;

  documents.removeOne(document);

  current = documents.isEmpty() ? nullptr : documents.last();

  delete document;

  emit documentChanged(current);
}