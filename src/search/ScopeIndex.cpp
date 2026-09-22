#include "../../include/search/ScopeIndex.h"

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

constexpr const char *kIndexFile = "faiss.index";
constexpr const char *kSidecarFile = "sidecar.json";
constexpr int kMinBodyLength = 24;

// Strip leading '#' characters and surrounding whitespace from a
// heading line. Returns empty for a line that is not a heading.
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

// Find the first line in [start, end) that begins with '#'. Returns the
// cleaned heading text, or empty if none.
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

ScopeIndex::ScopeIndex(InferenceService *inference, QObject *parent)
    : QObject(parent), m_inference(inference),
      m_vectors(std::make_unique<VectorIndex>()) {}

ScopeIndex::~ScopeIndex() = default;

void ScopeIndex::setIndexDirectory(const QString &directory) {
  m_indexDirectory = directory;
}

bool ScopeIndex::load() {
  if (m_indexDirectory.isEmpty()) {
    qWarning() << "[ScopeIndex] No index directory set";
    return false;
  }

  const QString indexPath =
      QDir(m_indexDirectory).filePath(kIndexFile);
  const QString sidecarPath =
      QDir(m_indexDirectory).filePath(kSidecarFile);

  if (!QFileInfo::exists(indexPath) || !QFileInfo::exists(sidecarPath)) {
    qDebug() << "[ScopeIndex] No existing index to load";
    return false;
  }

  if (!m_vectors->load(indexPath)) {
    qWarning() << "[ScopeIndex] Failed to load FAISS index";
    return false;
  }

  if (!loadSidecar(sidecarPath)) {
    qWarning() << "[ScopeIndex] Failed to load sidecar";
    m_vectors->clear();
    return false;
  }

  if (m_vectors->size() != m_entries.size()) {
    qWarning() << "[ScopeIndex] Vector count" << m_vectors->size()
               << "does not match sidecar" << m_entries.size()
               << "— discarding";
    m_vectors->clear();
    m_entries.clear();
    return false;
  }

  qDebug() << "[ScopeIndex] Loaded" << m_vectors->size() << "vectors";
  return true;
}

QStringList ScopeIndex::collectMarkdownFiles(const QString &root) const {
  QStringList files;

  QDirIterator it(root, QStringList{QStringLiteral("*.md")},
                  QDir::Files | QDir::Readable,
                  QDirIterator::Subdirectories);

  while (it.hasNext()) {
    files.append(it.next());
  }

  files.sort(Qt::CaseInsensitive);
  return files;
}

QVector<ScopeIndex::Entry> ScopeIndex::extractScopes(
    const QString &absolutePath) const {
  QVector<Entry> entries;

  QFile file(absolutePath);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    qWarning() << "[ScopeIndex] Cannot read" << absolutePath;
    return entries;
  }

  const QString text = QString::fromUtf8(file.readAll());
  file.close();

  if (text.trimmed().isEmpty()) {
    return entries;
  }

  // Run the Markdown parser directly. DocumentStructure on its own
  // only stores text; the tree is produced by the parser.
  MarkdownStructureParser parser;
  const DocumentStructure structure = parser.parse(text);

  const DocumentNode &root = structure.root();

  if (!root.isValid()) {
    return entries;
  }

  collectNodes(root, text, absolutePath, entries);

  return entries;
}

void ScopeIndex::collectNodes(const DocumentNode &node,
                              const QString &documentText,
                              const QString &filePath,
                              QVector<Entry> &out) const {
  if (node.isRoot() && node.isValid()) {
    // The root node covers the whole document. Emit it only if it has
    // content beyond any children. We skip it for now: the sections
    // below carry the retrievable text.
  } else if (node.isValid() && !node.id.isEmpty()) {
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

int ScopeIndex::rebuild(const QString &notesRoot) {
  if (m_indexDirectory.isEmpty()) {
    qWarning() << "[ScopeIndex] No index directory set";
    emit finished(-1);
    return -1;
  }

  if (!m_inference || !m_inference->isEmbedderReady()) {
    qWarning() << "[ScopeIndex] Embedder is not ready";
    emit finished(-1);
    return -1;
  }

  m_dimensions = m_inference->embedderDimensions();
  if (m_dimensions <= 0) {
    qWarning() << "[ScopeIndex] Invalid embedder dimensions";
    emit finished(-1);
    return -1;
  }

  if (!m_vectors->create(m_dimensions)) {
    qWarning() << "[ScopeIndex] Failed to create FAISS index";
    emit finished(-1);
    return -1;
  }

  m_entries.clear();

  const QStringList files = collectMarkdownFiles(notesRoot);

  qDebug() << "[ScopeIndex] Rebuilding from" << files.size() << "files";

  int processed = 0;
  const int total = files.size();

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

  QDir().mkpath(m_indexDirectory);

  const QString indexPath =
      QDir(m_indexDirectory).filePath(kIndexFile);
  const QString sidecarPath =
      QDir(m_indexDirectory).filePath(kSidecarFile);

  if (!m_vectors->save(indexPath)) {
    qWarning() << "[ScopeIndex] Failed to save FAISS index";
    emit finished(-1);
    return -1;
  }

  if (!saveSidecar(sidecarPath)) {
    qWarning() << "[ScopeIndex] Failed to save sidecar";
    emit finished(-1);
    return -1;
  }

  const int scopes = static_cast<int>(m_entries.size());
  qDebug() << "[ScopeIndex] Rebuild complete:" << scopes << "scopes";

  emit finished(scopes);
  return scopes;
}

bool ScopeIndex::isReady() const {
  return m_vectors && m_vectors->isValid() && m_vectors->size() > 0;
}

int64_t ScopeIndex::vectorCount() const {
  return m_vectors ? m_vectors->size() : 0;
}

int64_t ScopeIndex::scopeCount() const {
  return m_entries.size();
}

ScopeIndex::Entry ScopeIndex::entryFor(int64_t vectorId) const {
  if (vectorId < 0 || vectorId >= m_entries.size()) {
    return {};
  }
  return m_entries.at(static_cast<int>(vectorId));
}

bool ScopeIndex::saveSidecar(const QString &path) const {
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

bool ScopeIndex::loadSidecar(const QString &path) {
  QFile file(path);
  if (!file.open(QIODevice::ReadOnly)) {
    return false;
  }

  const QByteArray data = file.readAll();
  file.close();

  QJsonParseError error;
  const QJsonDocument document =
      QJsonDocument::fromJson(data, &error);

  if (error.error != QJsonParseError::NoError || !document.isObject()) {
    return false;
  }

  const QJsonObject root = document.object();

  if (root.value(QStringLiteral("version")).toInt() != kSidecarVersion) {
    qDebug() << "[ScopeIndex] Sidecar version mismatch";
    return false;
  }

  m_dimensions =
      root.value(QStringLiteral("dimensions")).toInt(m_dimensions);

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