#include "DocumentManager.h"

#include "Settings.h"
#include "media/MediaKind.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSaveFile>
#include <QTextStream>

namespace {

constexpr QLatin1StringView MarkdownExtension("md");
constexpr QLatin1StringView TextExtension("txt");
constexpr QLatin1StringView DotExtension("dot");
constexpr QLatin1StringView PlantUmlExtension("puml");
constexpr QLatin1StringView MermaidExtension("mmd");
constexpr QLatin1StringView UntitledBaseName("Untitled");

QString normalizedExtension(const QString &extension) {
  return extension.trimmed().toLower();
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

// Extensions that are binary by nature and should never be read as text.
// Opening them produces garbage and wastes memory. The list is
// deliberately conservative: it covers archives, executables, object
// files, and a few common opaque containers. Anything not on the list
// falls through to the existing text path.
bool isKnownBinaryExtension(const QString &extension) {
  static const QStringList kBinaryExtensions = {
      QStringLiteral("zip"),  QStringLiteral("tar"),
      QStringLiteral("gz"),   QStringLiteral("bz2"),
      QStringLiteral("xz"),   QStringLiteral("7z"),
      QStringLiteral("rar"),  QStringLiteral("zst"),
      QStringLiteral("tgz"),  QStringLiteral("tbz2"),
      QStringLiteral("exe"),  QStringLiteral("dll"),
      QStringLiteral("so"),   QStringLiteral("dylib"),
      QStringLiteral("o"),    QStringLiteral("obj"),
      QStringLiteral("a"),    QStringLiteral("lib"),
      QStringLiteral("class"), QStringLiteral("jar"),
      QStringLiteral("pyc"),  QStringLiteral("pyo"),
      QStringLiteral("wasm"), QStringLiteral("bin"),
      QStringLiteral("iso"),  QStringLiteral("img"),
      QStringLiteral("dmg"),  QStringLiteral("deb"),
      QStringLiteral("rpm"),  QStringLiteral("apk"),
      QStringLiteral("msi"),  QStringLiteral("sqlite"),
      QStringLiteral("db"),   QStringLiteral("dat"),
  };

  return kBinaryExtensions.contains(extension.trimmed().toLower());
}

} // namespace

DocumentManager::DocumentManager(QObject *parent)
    : QObject(parent), current(nullptr) {}

TextDocument *DocumentManager::currentDocument() const { return current; }

void DocumentManager::setCurrentDocument(TextDocument *document) {
  if (current == document) {
    return;
  }

  // Only accept documents that are actually open.
  if (document && !openDocumentsList.contains(document)) {
    return;
  }

  current = document;

  emit currentDocumentChanged(current);
  emit documentChanged(current);
}

DocumentMode DocumentManager::typeForExtension(const QString &extension) {
  const auto normalized = normalizedExtension(extension);

  if (normalized == MarkdownExtension) {
    return DocumentMode::Markdown;
  }
  if (normalized == "dot" || normalized == "gv") {
    return DocumentMode::Dot;
  }
  if (normalized == "puml" || normalized == "plantuml") {
    return DocumentMode::PlantUml;
  }
  if (normalized == MermaidExtension || normalized == "mermaid") {
    return DocumentMode::Mermaid;
  }
  if (normalized == "html" || normalized == "htm") {
    return DocumentMode::Html;
  }
  return DocumentMode::PlainText;
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

void DocumentManager::registerOpenDocument(TextDocument *document) {
  if (!document || openDocumentsList.contains(document)) {
    return;
  }

  openDocumentsList.append(document);

  emit documentOpened(document);
}

void DocumentManager::unregisterOpenDocument(TextDocument *document) {
  if (!document) {
    return;
  }

  const bool wasCurrent = (current == document);
  const int index = openDocumentsList.indexOf(document);

  if (index >= 0) {
    openDocumentsList.removeAt(index);
  }

  // Pick a neighbour to become current if we removed the current one.
  if (wasCurrent) {
    if (openDocumentsList.isEmpty()) {
      current = nullptr;
    } else {
      const int nextIndex = qBound(0, index, openDocumentsList.size() - 1);
      current = openDocumentsList.at(nextIndex);
    }
  }

  emit documentClosed(document);

  if (wasCurrent) {
    emit currentDocumentChanged(current);
    emit documentChanged(current);
  }
}

TextDocument *DocumentManager::createDocument(DocumentMode type,
                                              const QString &extension) {
  const QString path =
      uniqueDefaultPath(QString::fromLatin1(UntitledBaseName), extension);

  if (!writeTextFile(path, {})) {
    return nullptr;
  }

  auto *document = new TextDocument(this);

  document->setType(type);
  document->setFilePath(path);
  document->setModified(false);

  allDocuments.append(document);

  registerOpenDocument(document);

  current = document;
  emit currentDocumentChanged(current);
  emit documentChanged(current);
  emit documentCreated(path);

  return document;
}

TextDocument *DocumentManager::createDocumentIn(DocumentMode type,
                                                const QString &extension,
                                                const QString &parentPath) {
  QDir dir(parentPath);
  if (parentPath.isEmpty() || !dir.exists()) {
    return createDocument(type, extension);
  }

  const QString path =
      uniquePathIn(dir, QString::fromLatin1(UntitledBaseName), extension);

  if (!writeTextFile(path, {})) {
    return nullptr;
  }

  auto *document = new TextDocument(this);

  document->setType(type);
  document->setFilePath(path);
  document->setModified(false);

  allDocuments.append(document);

  registerOpenDocument(document);

  current = document;
  emit currentDocumentChanged(current);
  emit documentChanged(current);
  emit documentCreated(path);

  return document;
}

void DocumentManager::newTextFile() {
  createDocument(DocumentMode::PlainText, QString::fromLatin1(TextExtension));
}

void DocumentManager::newMarkdownFile() {
  createDocument(DocumentMode::Markdown,
                 QString::fromLatin1(MarkdownExtension));
}

void DocumentManager::newPlantUmlFile() {
  createDocument(DocumentMode::PlantUml,
                 QString::fromLatin1(PlantUmlExtension));
}

void DocumentManager::newTextFileIn(const QString &parentPath) {
  createDocumentIn(DocumentMode::PlainText,
                   QString::fromLatin1(TextExtension), parentPath);
}

void DocumentManager::newMarkdownFileIn(const QString &parentPath) {
  createDocumentIn(DocumentMode::Markdown,
                   QString::fromLatin1(MarkdownExtension), parentPath);
}

void DocumentManager::newPlantUmlFileIn(const QString &parentPath) {
  createDocumentIn(DocumentMode::PlantUml,
                   QString::fromLatin1(PlantUmlExtension), parentPath);
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

TextDocument *DocumentManager::openDocumentFromPath(const QString &path) {
  // If the path is already open, just focus it.
  const QString absolute = QFileInfo(path).absoluteFilePath();

  for (TextDocument *document : std::as_const(openDocumentsList)) {
    if (document->filePath() == absolute ||
        document->filePath() == path) {
      setCurrentDocument(document);
      return document;
    }
  }

  QString text;

  if (!readTextFile(path, text)) {
    return nullptr;
  }

  auto *document = new TextDocument(this);

  document->setPlainText(text);
  document->setType(typeForExtension(QFileInfo(path).suffix()));
  document->setFilePath(path);
  document->setModified(false);

  allDocuments.append(document);

  registerOpenDocument(document);

  current = document;
  emit currentDocumentChanged(current);
  emit documentChanged(current);

  return document;
}

bool DocumentManager::openFile(const QString &path) {
  const QFileInfo info(path);

  if (!info.exists() || !info.isFile()) {
    return false;
  }

  if (MediaKinds::isMediaPath(path)) {
    emit mediaFileRequested(info.absoluteFilePath());
    return true;
  }

  if (isKnownBinaryExtension(info.suffix())) {
    emit unsupportedFileRequested(
        info.absoluteFilePath(),
        tr("This file type is not supported: %1").arg(info.fileName()));
    return false;
  }

  return openDocumentFromPath(path) != nullptr;
}

bool DocumentManager::save() { return saveDocument(current); }

bool DocumentManager::saveDocument(TextDocument *document) {
  if (!document) {
    qWarning() << "[DOCUMENT] No document to save.";
    return false;
  }

  const QString path = document->filePath();

  if (path.isEmpty()) {
    qWarning() << "[DOCUMENT] Document has no file path.";
    return false;
  }

  if (!writeTextFile(path, document->toPlainText())) {
    return false;
  }

  document->setModified(false);

  if (document == current) {
    emit documentChanged(current);
  }

  emit documentSaved(document);

  return true;
}
bool DocumentManager::renameFile(const QString &oldPath,
                                 const QString &newPath) {
  if (oldPath == newPath) {
    return true;
  }

  for (TextDocument *document : std::as_const(allDocuments)) {
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

  // Close any open tabs whose path matches or is under the deleted path.
  QList<TextDocument *> toClose;

  for (TextDocument *document : std::as_const(openDocumentsList)) {
    const QString documentPath = document->filePath();

    if (documentPath == path ||
        (!info.isDir() &&
         QFileInfo(documentPath).absoluteFilePath() ==
             info.absoluteFilePath()) ||
        (info.isDir() && documentPath.startsWith(path + QDir::separator()))) {
      toClose.append(document);
    }
  }

  for (TextDocument *document : std::as_const(toClose)) {
    unregisterOpenDocument(document);
    allDocuments.removeOne(document);
    delete document;
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
  const QString targetPath = uniquePathIn(dir, baseName, targetExtension);

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

  for (TextDocument *document : std::as_const(allDocuments)) {
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

bool DocumentManager::convertToDot(const QString &path) {
  return convertFile(path, QString::fromLatin1(DotExtension),
                     DocumentMode::Dot);
}

bool DocumentManager::convertToMarkdown(const QString &path) {
  return convertFile(path, QString::fromLatin1(MarkdownExtension),
                     DocumentMode::Markdown);
}

bool DocumentManager::convertToText(const QString &path) {
  return convertFile(path, QString::fromLatin1(TextExtension),
                     DocumentMode::PlainText);
}

bool DocumentManager::convertToPlantUml(const QString &path) {
  return convertFile(path, QString::fromLatin1(PlantUmlExtension),
                     DocumentMode::PlantUml);
}

void DocumentManager::closeCurrent() { closeDocument(current); }

void DocumentManager::closeDocument(TextDocument *document) {
  if (!document) {
    return;
  }

  unregisterOpenDocument(document);
  allDocuments.removeOne(document);
  delete document;
}