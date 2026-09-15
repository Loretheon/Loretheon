#include "../../include/overseer/OverseerStorage.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTextStream>

namespace {

constexpr auto MemoryFilename = "memory.md";
constexpr auto SessionsDirname = "Sessions";

QString readTextFile(const QString &path) {
  QFile file(path);

  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return {};
  }

  QTextStream stream(&file);
  stream.setEncoding(QStringConverter::Utf8);

  const QString text = stream.readAll();

  if (stream.status() != QTextStream::Ok) {
    return {};
  }

  return text;
}

bool writeTextFile(const QString &path, const QString &text) {
  QFile file(path);

  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate |
                 QIODevice::Text)) {
    return false;
  }

  QTextStream stream(&file);
  stream.setEncoding(QStringConverter::Utf8);
  stream << text;

  if (stream.status() != QTextStream::Ok) {
    return false;
  }

  return true;
}

} // namespace

namespace OverseerStorage {

QString rootPath() {
  const QString base =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

  return QDir(base).filePath(QStringLiteral("Overseer"));
}

QString memoryPath() {
  return QDir(rootPath()).filePath(QString::fromLatin1(MemoryFilename));
}

QString readMemory() {
  return readTextFile(memoryPath());
}

bool writeMemory(const QString &text) {
  if (!ensureRoot()) {
    return false;
  }

  return writeTextFile(memoryPath(), text);
}

bool ensureRoot() {
  QDir dir;

  const QString root = rootPath();

  if (!dir.mkpath(root)) {
    return false;
  }

  const QString sessions =
      QDir(root).filePath(QString::fromLatin1(SessionsDirname));

  if (!dir.mkpath(sessions)) {
    return false;
  }

  if (!QFileInfo::exists(memoryPath())) {
    const QString header = QStringLiteral(
        "# Memory\n\n"
        "Standing facts the assistant should know in every session.\n\n");

    if (!writeTextFile(memoryPath(), header)) {
      return false;
    }
  }

  return true;
}

} // namespace OverseerStorage