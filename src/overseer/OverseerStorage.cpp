#include "../../include/overseer/OverseerStorage.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QStandardPaths>
#include <QStringList>
#include <QTextStream>

namespace {

constexpr auto MemoryFilename = "memory.md";
constexpr auto SessionsDirname = "Sessions";

const QString kProseEndMarker = QStringLiteral("## Accepted proposals");

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

struct ParsedMemory {
  QString prose;
  QStringList facts;
  QStringList rawFacts;
  bool hadSection = false;
};

ParsedMemory parseMemory(const QString &text) {
  ParsedMemory parsed;

  const QStringList lines = text.split(QChar('\n'));

  int sectionStart = -1;

  for (int i = 0; i < lines.size(); ++i) {
    if (lines.at(i).trimmed().startsWith(kProseEndMarker)) {
      sectionStart = i;
      break;
    }
  }

  QStringList proseLines;
  QStringList factLines;

  if (sectionStart < 0) {
    proseLines = lines;
  } else {
    for (int i = 0; i < sectionStart; ++i)
      proseLines.append(lines.at(i));

    for (int i = sectionStart + 1; i < lines.size(); ++i)
      factLines.append(lines.at(i));

    parsed.hadSection = true;
  }

  parsed.prose = proseLines.join(QChar('\n'));

  for (const QString &line : factLines) {
    parsed.rawFacts.append(line);

    const QString trimmed = line.trimmed();

    if (trimmed.startsWith(QStringLiteral("- ")))
      parsed.facts.append(trimmed.mid(2).trimmed());
  }

  return parsed;
}

QString serializeMemory(const ParsedMemory &parsed) {
  QString out = parsed.prose;

  while (out.endsWith(QChar('\n')))
    out.chop(1);

  if (!out.isEmpty())
    out += QChar('\n');

  out += kProseEndMarker;
  out += QChar('\n');

  for (const QString &line : parsed.rawFacts) {
    out += line;
    out += QChar('\n');
  }

  return out;
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

QString readMemory() { return readTextFile(memoryPath()); }

bool writeMemory(const QString &text) {
  if (!ensureRoot()) {
    return false;
  }

  return writeTextFile(memoryPath(), text);
}

bool appendFactToMemoryFile(const QString &path, const QString &fact) {
  const QString trimmed = fact.trimmed();

  if (path.isEmpty() || trimmed.isEmpty()) {
    return false;
  }

  const QString existing = readTextFile(path);

  ParsedMemory parsed = parseMemory(existing);

  if (!parsed.hadSection && parsed.prose.trimmed().isEmpty()) {
    parsed.prose =
        QStringLiteral("# Memory\n\n"
                       "Standing facts the assistant should know.\n");
  }

  parsed.rawFacts.append(QStringLiteral("- %1").arg(trimmed));
  parsed.facts.append(trimmed);

  return writeTextFile(path, serializeMemory(parsed));
}

bool removeFactFromMemoryFile(const QString &path, const QString &fact) {
  const QString trimmed = fact.trimmed();

  if (path.isEmpty() || trimmed.isEmpty()) {
    return false;
  }

  const QString existing = readTextFile(path);

  if (existing.isEmpty())
    return false;

  ParsedMemory parsed = parseMemory(existing);

  int removed = -1;

  for (int i = 0; i < parsed.rawFacts.size(); ++i) {
    const QString line = parsed.rawFacts.at(i).trimmed();

    if (!line.startsWith(QStringLiteral("- ")))
      continue;

    if (line.mid(2).trimmed() == trimmed) {
      removed = i;
      break;
    }
  }

  if (removed < 0)
    return false;

  parsed.rawFacts.removeAt(removed);

  return writeTextFile(path, serializeMemory(parsed));
}

bool replaceFactInMemoryFile(const QString &path, const QString &oldFact,
                             const QString &newFact) {
  const QString oldTrimmed = oldFact.trimmed();
  const QString newTrimmed = newFact.trimmed();

  if (path.isEmpty())
    return false;

  if (oldTrimmed.isEmpty() && newTrimmed.isEmpty())
    return false;

  // No old fact to remove: a plain append.
  if (oldTrimmed.isEmpty())
    return appendFactToMemoryFile(path, newTrimmed);

  // No new fact to write: a plain remove.
  if (newTrimmed.isEmpty())
    return removeFactFromMemoryFile(path, oldTrimmed);

  const QString existing = readTextFile(path);

  if (existing.isEmpty())
    return false;

  ParsedMemory parsed = parseMemory(existing);

  int target = -1;

  for (int i = 0; i < parsed.rawFacts.size(); ++i) {
    const QString line = parsed.rawFacts.at(i).trimmed();

    if (!line.startsWith(QStringLiteral("- ")))
      continue;

    if (line.mid(2).trimmed() == oldTrimmed) {
      target = i;
      break;
    }
  }

  // The old fact is not present. Refuse rather than silently appending;
  // a silent append is how two contradicting facts end up coexisting
  // and how the user never learns the replace failed.
  if (target < 0)
    return false;

  parsed.rawFacts[target] = QStringLiteral("- %1").arg(newTrimmed);

  return writeTextFile(path, serializeMemory(parsed));
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