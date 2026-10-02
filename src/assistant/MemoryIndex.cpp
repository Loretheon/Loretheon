#include "../../include/assistant/MemoryIndex.h"

#include "../../include/search/VectorIndex.h"

#include "../../include/text/structure/DocumentNode.h"
#include "../../include/text/structure/DocumentStructure.h"
#include "../../include/text/structure/MarkdownStructureParser.h"
#include "inference/InferenceService.h"

#include <QDebug>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace {

constexpr const char *kIndexDir = ".index";
constexpr const char *kIndexFile = "faiss.index";
constexpr const char *kSidecarFile = "sidecar.json";
constexpr int kMinBodyLength = 12;

QString cleanHeading(const QString &line) {
  QString trimmed = line.trimmed();
  if (!trimmed.startsWith(QChar('#'))) {
    return {};
  }
  while (trimmed.startsWith(QChar('#'))) {
    trimmed.remove(0, 1);
  }
  return trimmed.trimmed();
}

QString headingForRange(const QString &text, int start, int end) {
  const int safeStart = qBound(0, start, text.size());
  const int safeEnd = qBound(safeStart, end, text.size());

  if (safeEnd <= safeStart) {
    return {};
  }

  int cursor = safeStart;

  while (cursor < safeEnd) {
    int lineEnd = text.indexOf(QChar('\n'), cursor);
    if (lineEnd < 0 || lineEnd > safeEnd) {
      lineEnd = safeEnd;
    }

    const QString heading =
        cleanHeading(text.mid(cursor, lineEnd - cursor));

    if (!heading.isEmpty()) {
      return heading;
    }

    cursor = lineEnd + 1;
  }

  return {};
}

} // namespace

MemoryIndex::MemoryIndex(InferenceService *inference, QObject *parent)
    : QObject(parent), m_inference(inference),
      m_vectors(std::make_unique<VectorIndex>()) {}

MemoryIndex::~MemoryIndex() = default;

void MemoryIndex::setMemoryRoot(const QString &root) { m_memoryRoot = root; }

QString MemoryIndex::indexDirectory() const {
  if (m_memoryRoot.isEmpty()) {
    return {};
  }
  return QDir(m_memoryRoot).filePath(QString::fromLatin1(kIndexDir));
}

QStringList MemoryIndex::collectMarkdownFiles() const {
  QStringList files;

  if (m_memoryRoot.isEmpty() || !QFileInfo::exists(m_memoryRoot)) {
    return files;
  }

  QDirIterator it(m_memoryRoot, QStringList{QStringLiteral("*.md")},
                  QDir::Files | QDir::Readable,
                  QDirIterator::Subdirectories);

  while (it.hasNext()) {
    const QString path = it.next();

    if (path.contains(QStringLiteral("/.index/"))) {
      continue;
    }

    files.append(path);
  }

  files.sort(Qt::CaseInsensitive);
  return files;
}

QVector<MemoryIndex::Entry> MemoryIndex::extractScopes(
    const QString &absolutePath) const {
  QVector<Entry> entries;

  QFile file(absolutePath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return entries;
  }

  const QString text = QString::fromUtf8(file.readAll());
  file.close();

  if (text.trimmed().isEmpty()) {
    return entries;
  }

  MarkdownStructureParser parser;
  const DocumentStructure structure = parser.parse(text);

  const DocumentNode &root = structure.root();

  if (!root.isValid()) {
    if (text.trimmed().length() >= kMinBodyLength) {
      Entry entry;
      entry.filePath = absolutePath;
      entry.scopeId = QStringLiteral("document");
      entry.body = text.trimmed();
      entry.contentHash = QString();
      entries.append(entry);
    }
    return entries;
  }

  collectNodes(root, text, absolutePath, entries);

  if (entries.isEmpty() && text.trimmed().length() >= kMinBodyLength) {
    Entry entry;
    entry.filePath = absolutePath;
    entry.scopeId = QStringLiteral("document");
    entry.body = text.trimmed();
    entry.contentHash = QString();
    entries.append(entry);
  }

  return entries;
}

void MemoryIndex::collectNodes(const DocumentNode &node,
                               const QString &documentText,
                               const QString &filePath,
                               QVector<Entry> &out) const {
  if (node.isRoot() && node.isValid()) {
    if (documentText.trimmed().length() >= kMinBodyLength) {
      Entry entry;
      entry.filePath = filePath;
      entry.scopeId = QStringLiteral("document");
      entry.body = documentText.trimmed();
      entry.contentHash = node.contentHash;
      out.append(entry);
    }
    return;
  }

  if (node.isValid() && !node.id.isEmpty()) {
    const int start = qBound(0, node.start, documentText.size());
    const int end = qBound(start, node.end, documentText.size());

    if (end > start) {
      const QString body = documentText.mid(start, end - start).trimmed();

      if (body.length() >= kMinBodyLength) {
        Entry entry;
        entry.filePath = filePath;
        entry.scopeId = node.id;
        entry.heading = headingForRange(documentText, start, end);
        entry.body = body;
        entry.contentHash = node.contentHash;
        out.append(entry);
      }
    }
  }

  for (const DocumentNode &child : node.children) {
    collectNodes(child, documentText, filePath, out);
  }
}

bool MemoryIndex::load() {
  const QString dir = indexDirectory();

  if (dir.isEmpty()) {
    return false;
  }

  const QString indexPath = QDir(dir).filePath(kIndexFile);
  const QString sidecarPath = QDir(dir).filePath(kSidecarFile);

  if (!QFileInfo::exists(indexPath) || !QFileInfo::exists(sidecarPath)) {
    return false;
  }

  if (!m_vectors->load(indexPath)) {
    return false;
  }

  if (!loadSidecar(sidecarPath)) {
    m_vectors->clear();
    return false;
  }

  if (m_vectors->size() != m_entries.size()) {
    m_vectors->clear();
    m_entries.clear();
    return false;
  }

  qDebug() << "[MemoryIndex] Loaded" << m_vectors->size() << "vectors";
  return true;
}

int MemoryIndex::rebuild() {
  const QString dir = indexDirectory();

  if (dir.isEmpty()) {
    emit finished(-1);
    return -1;
  }

  if (!m_inference || !m_inference->isEmbedderReady()) {
    emit finished(-1);
    return -1;
  }

  m_dimensions = m_inference->embedderDimensions();

  if (m_dimensions <= 0) {
    emit finished(-1);
    return -1;
  }

  if (!m_vectors->create(m_dimensions)) {
    emit finished(-1);
    return -1;
  }

  m_entries.clear();

  const QStringList files = collectMarkdownFiles();

  int processed = 0;
  const int total = qMax(1, files.size());

  for (const QString &path : files) {
    const QVector<Entry> scopes = extractScopes(path);

    for (const Entry &entry : scopes) {
      const std::vector<float> vector = m_inference->embed(entry.body);

      if (vector.empty()) {
        continue;
      }

      const int64_t id = m_vectors->add(vector);

      if (id < 0) {
        continue;
      }

      m_entries.append(entry);
    }

    ++processed;
    emit progress(processed, total);
  }

  QDir().mkpath(dir);

  if (!saveIndex() || !saveSidecar(QDir(dir).filePath(kSidecarFile))) {
    emit finished(-1);
    return -1;
  }

  const int scopes = static_cast<int>(m_entries.size());
  qDebug() << "[MemoryIndex] Rebuild complete:" << scopes << "scopes";

  emit finished(scopes);
  return scopes;
}

int MemoryIndex::refreshFile(const QString &absolutePath) {
  if (absolutePath.isEmpty()) {
    return -1;
  }

  const QString dir = indexDirectory();

  if (dir.isEmpty()) {
    return -1;
  }

  if (!m_inference || !m_inference->isEmbedderReady()) {
    return -1;
  }

  if (!m_vectors || !m_vectors->isValid()) {
    return -1;
  }

  const QString canonical = QFileInfo(absolutePath).absoluteFilePath();

  // Remove existing entries for this file. VectorIndex::removeIds
  // compacts the underlying storage and reassigns ids sequentially,
  // so the sidecar entries must be rebuilt in the same pass.

  QVector<int64_t> idsToRemove;
  QVector<Entry> surviving;

  surviving.reserve(m_entries.size());

  for (int i = 0; i < m_entries.size(); ++i) {
    const Entry &entry = m_entries.at(i);

    if (entry.filePath == canonical) {
      idsToRemove.append(i);
    } else {
      surviving.append(entry);
    }
  }

  if (!idsToRemove.isEmpty()) {
    m_vectors->removeIds(idsToRemove);
  }

  m_entries = surviving;

  // Re-extract the file from disk.

  const QVector<Entry> scopes = extractScopes(canonical);

  int added = 0;

  for (const Entry &entry : scopes) {
    const std::vector<float> vector = m_inference->embed(entry.body);

    if (vector.empty()) {
      continue;
    }

    const int64_t id = m_vectors->add(vector);

    if (id < 0) {
      continue;
    }

    m_entries.append(entry);
    ++added;
  }

  if (!saveIndex() || !saveSidecar(QDir(dir).filePath(kSidecarFile))) {
    return -1;
  }

  return added;
}

bool MemoryIndex::isReady() const {
  return m_vectors && m_vectors->isValid() && m_vectors->size() > 0;
}

int64_t MemoryIndex::vectorCount() const {
  return m_vectors ? m_vectors->size() : 0;
}

int64_t MemoryIndex::scopeCount() const { return m_entries.size(); }

MemoryIndex::Entry MemoryIndex::entryFor(int64_t vectorId) const {
  if (vectorId < 0 || vectorId >= m_entries.size()) {
    return {};
  }
  return m_entries.at(static_cast<int>(vectorId));
}

bool MemoryIndex::saveIndex() const {
  const QString dir = indexDirectory();

  if (dir.isEmpty() || !m_vectors) {
    return false;
  }

  QDir().mkpath(dir);

  return m_vectors->save(QDir(dir).filePath(kIndexFile));
}

bool MemoryIndex::saveSidecar(const QString &path) const {
  QJsonArray array;

  for (const Entry &entry : m_entries) {
    QJsonObject object;
    object.insert(QStringLiteral("file"), entry.filePath);
    object.insert(QStringLiteral("scope"), entry.scopeId);
    object.insert(QStringLiteral("heading"), entry.heading);
    object.insert(QStringLiteral("body"), entry.body);
    object.insert(QStringLiteral("hash"), entry.contentHash);
    array.append(object);
  }

  QJsonObject root;
  root.insert(QStringLiteral("version"), kSidecarVersion);
  root.insert(QStringLiteral("dimensions"), m_dimensions);
  root.insert(QStringLiteral("entries"), array);

  QFile file(path);

  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
    return false;
  }

  file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
  file.close();
  return true;
}

bool MemoryIndex::loadSidecar(const QString &path) {
  QFile file(path);

  if (!file.open(QIODevice::ReadOnly)) {
    return false;
  }

  const QByteArray data = file.readAll();
  file.close();

  QJsonParseError error;
  const QJsonDocument document = QJsonDocument::fromJson(data, &error);

  if (error.error != QJsonParseError::NoError || !document.isObject()) {
    return false;
  }

  const QJsonObject root = document.object();

  if (root.value(QStringLiteral("version")).toInt() != kSidecarVersion) {
    return false;
  }

  m_dimensions = root.value(QStringLiteral("dimensions")).toInt(m_dimensions);

  const QJsonArray array = root.value(QStringLiteral("entries")).toArray();

  m_entries.clear();
  m_entries.reserve(array.size());

  for (const QJsonValue &value : array) {
    if (!value.isObject()) {
      return false;
    }

    const QJsonObject object = value.toObject();

    Entry entry;
    entry.filePath = object.value(QStringLiteral("file")).toString();
    entry.scopeId = object.value(QStringLiteral("scope")).toString();
    entry.heading = object.value(QStringLiteral("heading")).toString();
    entry.body = object.value(QStringLiteral("body")).toString();
    entry.contentHash = object.value(QStringLiteral("hash")).toString();

    m_entries.append(entry);
  }

  return true;
}