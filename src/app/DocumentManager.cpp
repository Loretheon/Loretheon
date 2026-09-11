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
  const QDir root(Settings::getRootDirectory());
  const QString normalized = normalizedExtension(extension);

  QString path =
      root.filePath(QStringLiteral("%1.%2").arg(baseName, normalized));

  for (int suffix = 2; QFile::exists(path); ++suffix) {
    path = root.filePath(
        QStringLiteral("%1 %2.%3").arg(baseName).arg(suffix).arg(normalized));
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

void DocumentManager::newTextFile() {
  createDocument(DocumentMode::PlainText, QString::fromLatin1(TextExtension));
}

void DocumentManager::newMarkdownFile() {
  createDocument(DocumentMode::Markdown,
                 QString::fromLatin1(MarkdownExtension));
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