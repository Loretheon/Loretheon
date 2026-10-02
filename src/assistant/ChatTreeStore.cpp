#include "../../include/assistant/ChatTreeStore.h"

#include "../../include/assistant/ChatTree.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QTextStream>
#include <QUuid>

namespace {

constexpr QChar kBlockSeparator(0x1E);
constexpr auto kChatsDir = "chats";

QString kindToString(ChatNode::Kind kind) {
  switch (kind) {
  case ChatNode::Kind::UserText: return QStringLiteral("user");
  case ChatNode::Kind::AssistantText: return QStringLiteral("assistant");
  case ChatNode::Kind::JobSearch: return QStringLiteral("job_search");
  case ChatNode::Kind::JobDelegate: return QStringLiteral("job_delegate");
  case ChatNode::Kind::JobPromote: return QStringLiteral("job_promote");
  case ChatNode::Kind::JobRead: return QStringLiteral("job_read");
  case ChatNode::Kind::JobEdit: return QStringLiteral("job_edit");
  case ChatNode::Kind::Tool: return QStringLiteral("tool");
  case ChatNode::Kind::Status: return QStringLiteral("status");
  case ChatNode::Kind::Error: return QStringLiteral("error");
  }
  return QStringLiteral("status");
}

ChatNode::Kind kindFromString(const QString &raw) {
  if (raw == QStringLiteral("user")) return ChatNode::Kind::UserText;
  if (raw == QStringLiteral("assistant")) return ChatNode::Kind::AssistantText;
  if (raw == QStringLiteral("job_search")) return ChatNode::Kind::JobSearch;
  if (raw == QStringLiteral("job_delegate")) return ChatNode::Kind::JobDelegate;
  if (raw == QStringLiteral("job_promote")) return ChatNode::Kind::JobPromote;
  if (raw == QStringLiteral("job_read")) return ChatNode::Kind::JobRead;
  if (raw == QStringLiteral("job_edit")) return ChatNode::Kind::JobEdit;
  if (raw == QStringLiteral("tool")) return ChatNode::Kind::Tool;
  if (raw == QStringLiteral("error")) return ChatNode::Kind::Error;
  return ChatNode::Kind::Status;
}

QString stateToString(ChatNode::State state) {
  switch (state) {
  case ChatNode::State::None: return QStringLiteral("none");
  case ChatNode::State::Pending: return QStringLiteral("pending");
  case ChatNode::State::Running: return QStringLiteral("running");
  case ChatNode::State::Done: return QStringLiteral("done");
  case ChatNode::State::Failed: return QStringLiteral("failed");
  case ChatNode::State::Cancelled: return QStringLiteral("cancelled");
  case ChatNode::State::Waiting: return QStringLiteral("waiting");
  }
  return QStringLiteral("none");
}

ChatNode::State stateFromString(const QString &raw) {
  if (raw == QStringLiteral("pending")) return ChatNode::State::Pending;
  if (raw == QStringLiteral("running")) return ChatNode::State::Running;
  if (raw == QStringLiteral("done")) return ChatNode::State::Done;
  if (raw == QStringLiteral("failed")) return ChatNode::State::Failed;
  if (raw == QStringLiteral("cancelled")) return ChatNode::State::Cancelled;
  if (raw == QStringLiteral("waiting")) return ChatNode::State::Waiting;
  return ChatNode::State::None;
}

QString serializeNode(const ChatNode &node) {
  QJsonObject payload;

  payload.insert(QStringLiteral("id"), node.id);
  payload.insert(QStringLiteral("parentId"), node.parentId);
  payload.insert(QStringLiteral("state"), stateToString(node.state));
  payload.insert(QStringLiteral("collapsed"), node.collapsed);

  if (!node.detail.isEmpty())
    payload.insert(QStringLiteral("detail"), node.detail);
  if (!node.jobId.isEmpty())
    payload.insert(QStringLiteral("jobId"), node.jobId);
  if (!node.result.isEmpty())
    payload.insert(QStringLiteral("result"), node.result);
  if (!node.error.isEmpty())
    payload.insert(QStringLiteral("error"), node.error);

  if (!node.children.isEmpty()) {
    QJsonArray children;
    for (const QString &child : node.children)
      children.append(child);
    payload.insert(QStringLiteral("children"), children);
  }

  const QString json = QString::fromUtf8(
      QJsonDocument(payload).toJson(QJsonDocument::Compact));

  QString block;
  block += QStringLiteral("## ");
  block += kindToString(node.kind);
  block += QStringLiteral(" | ");
  block += node.createdAt.toString(Qt::ISODateWithMs);
  block += QStringLiteral(" | ");
  block += json;
  block += QChar('\n');
  block += node.text;
  block += QChar('\n');
  block += kBlockSeparator;
  block += QChar('\n');

  return block;
}

bool parseHeader(const QString &line, ChatNode &node) {
  if (!line.startsWith(QStringLiteral("## ")))
    return false;

  const int firstPipe = line.indexOf(QChar('|'));
  if (firstPipe < 0)
    return false;

  const int secondPipe = line.indexOf(QChar('|'), firstPipe + 1);
  if (secondPipe < 0)
    return false;

  const QString kindString = line.mid(3, firstPipe - 3).trimmed();
  const QString timestampString =
      line.mid(firstPipe + 1, secondPipe - firstPipe - 1).trimmed();
  const QString json = line.mid(secondPipe + 1).trimmed();

  node.kind = kindFromString(kindString);

  node.createdAt =
      QDateTime::fromString(timestampString, Qt::ISODateWithMs);
  if (!node.createdAt.isValid())
    node.createdAt = QDateTime::currentDateTime();
  node.updatedAt = node.createdAt;

  const QJsonObject payload =
      QJsonDocument::fromJson(json.toUtf8()).object();

  node.id = payload.value(QStringLiteral("id")).toString();
  node.parentId = payload.value(QStringLiteral("parentId")).toString();
  node.state = stateFromString(
      payload.value(QStringLiteral("state")).toString());
  node.collapsed = payload.value(QStringLiteral("collapsed")).toBool();

  node.detail = payload.value(QStringLiteral("detail")).toString();
  node.jobId = payload.value(QStringLiteral("jobId")).toString();
  node.result = payload.value(QStringLiteral("result")).toString();
  node.error = payload.value(QStringLiteral("error")).toString();

  const QJsonArray children =
      payload.value(QStringLiteral("children")).toArray();
  for (const QJsonValue &value : children) {
    const QString child = value.toString();
    if (!child.isEmpty())
      node.children.append(child);
  }

  return !node.id.isEmpty();
}

} // namespace

ChatTreeStore::ChatTreeStore(QObject *parent) : QObject(parent) {}

void ChatTreeStore::setRoot(const QString &root) {
  m_root = root;
}

QString ChatTreeStore::dayDirectory(const QDate &date) const {
  if (m_root.isEmpty())
    return {};

  const QString name = date.toString(QStringLiteral("yyyy-MM-dd"));

  return QDir(m_root)
      .filePath(QDir(QString::fromLatin1(kChatsDir)).filePath(name));
}

QString ChatTreeStore::generateName() const {
  const QString raw = QUuid::createUuid().toString(QUuid::WithoutBraces);
  const QByteArray digest =
      QCryptographicHash::hash(raw.toUtf8(), QCryptographicHash::Sha1);
  return QString::fromLatin1(digest.toHex()).left(6);
}

QString ChatTreeStore::normaliseName(const QString &raw) {
  QString name = raw.trimmed().toLower();

  static const QRegularExpression unsafe(
      QStringLiteral("[^a-z0-9\\-_ ]+"));
  name.replace(unsafe, QStringLiteral(" "));

  static const QRegularExpression spaces(QStringLiteral("\\s+"));
  name.replace(spaces, QStringLiteral("-"));

  while (name.startsWith(QChar('-')))
    name.remove(0, 1);

  while (name.endsWith(QChar('-')))
    name.chop(1);

  if (name.isEmpty())
    name = QStringLiteral("chat");

  return name;
}

QString ChatTreeStore::uniquePathIn(const QString &directory,
                                    const QString &baseName,
                                    const QString &ignorePath) const {
  QDir dir(directory);

  const QString ignore = ignorePath.isEmpty()
                             ? QString()
                             : QFileInfo(ignorePath).absoluteFilePath();

  QString candidate =
      dir.filePath(baseName + QStringLiteral(".md"));

  if (QFileInfo(candidate).absoluteFilePath() == ignore)
    return candidate;

  for (int n = 2; QFileInfo::exists(candidate); ++n) {
    candidate = dir.filePath(
        QStringLiteral("%1-%2.md").arg(baseName).arg(n));

    if (QFileInfo(candidate).absoluteFilePath() == ignore)
      return candidate;
  }

  return candidate;
}

QString ChatTreeStore::beginNewSegment() {
  if (m_root.isEmpty())
    return {};

  m_currentDate = QDate::currentDate();
  m_currentName = generateName();

  const QString directory = dayDirectory(m_currentDate);
  m_currentPath = QDir(directory).filePath(m_currentName + QStringLiteral(".md"));

  QDir().mkpath(directory);

  QFile file(m_currentPath);

  if (file.open(QIODevice::WriteOnly | QIODevice::Truncate |
                QIODevice::Text)) {
    file.close();
  }

  emit segmentChanged(m_currentPath);
  return m_currentPath;
}

bool ChatTreeStore::openSegment(const QString &absolutePath) {
  if (absolutePath.isEmpty())
    return false;

  const QFileInfo info(absolutePath);

  if (!info.exists() || !info.isFile())
    return false;

  m_currentPath = info.absoluteFilePath();
  m_currentName = info.completeBaseName();

  const QDate fromName = QDate::fromString(info.dir().dirName(),
                                            QStringLiteral("yyyy-MM-dd"));
  m_currentDate = fromName.isValid() ? fromName : QDate::currentDate();

  emit segmentChanged(m_currentPath);
  return true;
}

bool ChatTreeStore::load(ChatTree *tree) {
  if (!tree)
    return false;

  tree->clear();

  if (m_currentPath.isEmpty() || !QFileInfo::exists(m_currentPath))
    return true;

  QFile file(m_currentPath);

  if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    return false;

  QTextStream stream(&file);
  stream.setEncoding(QStringConverter::Utf8);

  const QString text = stream.readAll();

  const QStringList blocks = text.split(kBlockSeparator, Qt::SkipEmptyParts);

  QVector<ChatNode> parsed;
  parsed.reserve(blocks.size());

  for (const QString &rawBlock : blocks) {
    QString block = rawBlock;

    while (block.startsWith(QChar('\n')))
      block.remove(0, 1);
    while (block.endsWith(QChar('\n')))
      block.chop(1);

    if (block.isEmpty())
      continue;

    const int firstNewline = block.indexOf(QChar('\n'));
    const QString firstLine =
        firstNewline < 0 ? block : block.left(firstNewline);
    const QString body =
        firstNewline < 0 ? QString() : block.mid(firstNewline + 1);

    ChatNode node;

    if (!parseHeader(firstLine.trimmed(), node))
      continue;

    node.text = body;
    parsed.append(node);
  }

  for (const ChatNode &node : parsed) {
    ChatNode copy = node;
    copy.children.clear();
    tree->appendWithId(copy, copy.id, copy.parentId);
  }

  for (const ChatNode &node : parsed) {
    ChatNode *existing = tree->node(node.id);
    if (!existing)
      continue;

    if (!node.detail.isEmpty())
      tree->setDetail(node.id, node.detail);
    if (!node.result.isEmpty())
      tree->setResult(node.id, node.result);
    if (!node.error.isEmpty())
      tree->setError(node.id, node.error);

    existing->state = node.state;
    existing->createdAt = node.createdAt;
    existing->updatedAt = node.updatedAt;
    existing->collapsed = node.collapsed;
    existing->text = node.text;
  }

  return true;
}

bool ChatTreeStore::save(const ChatTree *tree) {
  if (!tree || m_currentPath.isEmpty())
    return false;

  const QFileInfo info(m_currentPath);

  if (!QDir().mkpath(info.absolutePath()))
    return false;

  QFile file(m_currentPath);

  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate |
                 QIODevice::Text))
    return false;

  QTextStream stream(&file);
  stream.setEncoding(QStringConverter::Utf8);

  const auto emitSubtree = [&](auto &&self, const QString &id) -> void {
    const ChatNode *node = tree->node(id);

    if (!node)
      return;

    stream << serializeNode(*node);

    for (const QString &child : node->children)
      self(self, child);
  };

  for (const QString &root : tree->roots())
    emitSubtree(emitSubtree, root);

  stream.flush();

  return stream.status() == QTextStream::Ok;
}

QString ChatTreeStore::renameSegment(const QString &absolutePath,
                                     const QString &newName) {
  if (absolutePath.isEmpty())
    return {};

  const QFileInfo info(absolutePath);

  if (!info.exists() || !info.isFile())
    return {};

  const QString base = normaliseName(newName);

  const QString directory = info.absolutePath();

  const QString target = uniquePathIn(directory, base, absolutePath);

  if (target == info.absoluteFilePath())
    return target;

  if (!QFile::rename(info.absoluteFilePath(), target))
    return {};

  if (info.absoluteFilePath() == m_currentPath) {
    m_currentPath = target;
    m_currentName = QFileInfo(target).completeBaseName();

    emit segmentChanged(m_currentPath);
  }

  return target;
}

bool ChatTreeStore::deleteSegment(const QString &absolutePath) {
  if (absolutePath.isEmpty())
    return false;

  const QFileInfo info(absolutePath);

  if (!info.exists() || !info.isFile())
    return false;

  const QString removed = info.absoluteFilePath();

  if (!QFile::remove(removed))
    return false;

  if (removed == m_currentPath) {
    m_currentPath.clear();
    m_currentName.clear();
    m_currentDate = QDate();

    emit segmentChanged(QString());
  }

  return true;
}

QVector<ChatTreeStore::Entry> ChatTreeStore::listSegments() const {
  QVector<Entry> result;

  if (m_root.isEmpty())
    return result;

  const QString chatsRoot =
      QDir(m_root).filePath(QString::fromLatin1(kChatsDir));

  QDirIterator dayIt(chatsRoot, QDir::Dirs | QDir::NoDotAndDotDot);

  while (dayIt.hasNext()) {
    const QString dayPath = dayIt.next();

    const QDate date =
        QDate::fromString(QFileInfo(dayPath).fileName(),
                          QStringLiteral("yyyy-MM-dd"));

    if (!date.isValid())
      continue;

    QDirIterator fileIt(dayPath,
                        QStringList{QStringLiteral("*.md")},
                        QDir::Files | QDir::Readable);

    while (fileIt.hasNext()) {
      const QString path = fileIt.next();

      Entry entry;
      entry.absolutePath = path;
      entry.date = date;
      entry.name = QFileInfo(path).completeBaseName();

      result.append(entry);
    }
  }

  std::sort(result.begin(), result.end(),
            [](const Entry &a, const Entry &b) {
              if (a.date != b.date)
                return a.date > b.date;
              return a.name < b.name;
            });

  return result;
}