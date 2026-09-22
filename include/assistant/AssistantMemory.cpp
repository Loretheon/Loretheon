#include "../../include/assistant/AssistantMemory.h"

#include "../../include/search/SearchService.h"
#include "MemoryIndex.h"
#include "VectorIndex.h"
#include "inference/InferenceService.h"

#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QSaveFile>
#include <QTextStream>

namespace {

constexpr const char *kSessionsDir = "memories/sessions";
constexpr const char *kTopicsDir = "memories/topics";

} // namespace

void AssistantMemory::setRoot(const QString &root) { m_root = root; }

void AssistantMemory::setMemoryIndex(MemoryIndex *index) { m_index = index; }

void AssistantMemory::setInference(InferenceService *inference) {
  m_inference = inference;
}

bool AssistantMemory::ensureRoot() const {
  if (m_root.isEmpty()) {
    return false;
  }

  QDir dir(m_root);

  if (!dir.exists() && !dir.mkpath(QStringLiteral("."))) {
    return false;
  }

  if (!QDir().mkpath(QDir(m_root).filePath(kSessionsDir))) {
    return false;
  }

  if (!QDir().mkpath(QDir(m_root).filePath(kTopicsDir))) {
    return false;
  }

  return true;
}

QString AssistantMemory::sessionPath(const QDateTime &timestamp) const {
  const QString name =
      timestamp.toString(QStringLiteral("yyyy-MM-dd-HHmm")) +
      QStringLiteral(".md");

  return QDir(m_root).filePath(
      QDir(QString::fromLatin1(kSessionsDir)).filePath(name));
}

QString AssistantMemory::topicPath(const QString &slug) const {
  return QDir(m_root).filePath(
      QDir(QString::fromLatin1(kTopicsDir))
          .filePath(slug + QStringLiteral(".md")));
}

QStringList AssistantMemory::memoryFiles() const {
  QStringList files;

  const QString sessionsRoot = QDir(m_root).filePath(kSessionsDir);
  const QString topicsRoot = QDir(m_root).filePath(kTopicsDir);

  QDirIterator sessions(sessionsRoot, QStringList{QStringLiteral("*.md")},
                        QDir::Files | QDir::Readable);
  while (sessions.hasNext()) {
    files.append(sessions.next());
  }

  QDirIterator topics(topicsRoot, QStringList{QStringLiteral("*.md")},
                      QDir::Files | QDir::Readable);
  while (topics.hasNext()) {
    files.append(topics.next());
  }

  files.sort(Qt::CaseInsensitive);
  return files;
}

bool AssistantMemory::appendToTopic(const QString &slug,
                                    const QString &fact) {
  if (!ensureRoot()) {
    return false;
  }

  const QString path = topicPath(slug);

  // If the topic file does not exist, seed it with a header so the
  // file is legible when opened by hand.
  QFile check(path);
  const bool exists = check.exists();

  QFile file(path);

  if (!file.open(QIODevice::Append | QIODevice::Text)) {
    qWarning() << "[AssistantMemory] Cannot append to:" << path
               << file.errorString();
    return false;
  }

  QTextStream stream(&file);
  stream.setEncoding(QStringConverter::Utf8);

  if (!exists) {
    stream << "# Topic: " << slug << "\n\n";
  }

  stream << "## "
         << QDateTime::currentDateTimeUtc().toString(
                Qt::ISODate)
         << "\n\n"
         << fact.trimmed() << "\n\n";

  stream.flush();

  if (stream.status() != QTextStream::Ok) {
    qWarning() << "[AssistantMemory] Write failed:" << path;
    return false;
  }

  file.close();
  return true;
}

bool AssistantMemory::writeSession(const QDateTime &timestamp,
                                   const QString &summary) {
  if (!ensureRoot()) {
    return false;
  }

  const QString path = sessionPath(timestamp);

  QSaveFile file(path);

  if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
    qWarning() << "[AssistantMemory] Cannot open session for writing:"
               << path << file.errorString();
    return false;
  }

  QTextStream stream(&file);
  stream.setEncoding(QStringConverter::Utf8);

  stream << "# Session "
         << timestamp.toString(Qt::ISODate)
         << "\n\n"
         << summary.trimmed()
         << "\n";

  stream.flush();

  if (stream.status() != QTextStream::Ok) {
    file.cancelWriting();
    return false;
  }

  if (!file.commit()) {
    return false;
  }

  return true;
}

QStringList AssistantMemory::recall(const QString &query, int k) const {
  QStringList results;

  if (!m_index || !m_index->isReady() || query.trimmed().isEmpty() || k <= 0) {
    return results;
  }

  if (!m_inference || !m_inference->isEmbedderReady()) {
    return results;
  }

  const std::vector<float> queryVector = m_inference->embed(query);

  if (queryVector.empty()) {
    return results;
  }

  VectorIndex *vectors = m_index->vectors();

  if (!vectors) {
    return results;
  }

  const QVector<VectorIndex::Hit> hits = vectors->search(queryVector, k);

  for (const VectorIndex::Hit &hit : hits) {
    const MemoryIndex::Entry entry = m_index->entryFor(hit.id);

    if (entry.body.isEmpty()) {
      continue;
    }

    if (!results.contains(entry.body)) {
      results.append(entry.body);
    }
  }

  return results;
}
QString AssistantMemory::slugify(const QString &topic) {
  QString result = topic.trimmed().toLower();

  static const QRegularExpression nonAlnum(
      QStringLiteral("[^a-z0-9]+"));
  result.replace(nonAlnum, QStringLiteral("-"));

  while (result.startsWith(QLatin1Char('-'))) {
    result.remove(0, 1);
  }

  while (result.endsWith(QLatin1Char('-'))) {
    result.chop(1);
  }

  if (result.isEmpty()) {
    result = QStringLiteral("misc");
  }

  return result;
}

QString AssistantMemory::readFile(const QString &path) {
  QFile file(path);

  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return {};
  }

  QTextStream stream(&file);
  stream.setEncoding(QStringConverter::Utf8);
  return stream.readAll();
}