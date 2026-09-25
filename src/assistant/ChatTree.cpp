#include "../../include/assistant/ChatTree.h"

#include <QUuid>

ChatTree::ChatTree(QObject *parent) : QObject(parent) {}

void ChatTree::clear() {
  m_nodes.clear();
  m_jobToNode.clear();
  m_roots.clear();
  m_nextOrdinal = 1;

  emit cleared();
}

QString ChatTree::append(const ChatNode &node, const QString &parentId) {
  const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  return appendWithId(node, id, parentId);
}

QString ChatTree::appendWithId(const ChatNode &node, const QString &id,
                               const QString &parentId) {
  if (id.isEmpty() || m_nodes.contains(id)) {
    return {};
  }

  ChatNode copy = node;
  copy.id = id;
  copy.parentId = parentId;
  copy.createdAt = QDateTime::currentDateTime();
  copy.updatedAt = copy.createdAt;

  m_nodes.insert(copy.id, copy);

  if (!copy.jobId.isEmpty()) {
    m_jobToNode.insert(copy.jobId, copy.id);
  }

  if (parentId.isEmpty()) {
    m_roots.append(copy.id);
  } else {
    auto it = m_nodes.find(parentId);

    if (it != m_nodes.end()) {
      it->children.append(copy.id);
      it->updatedAt = QDateTime::currentDateTime();

      emit nodeChanged(parentId);
    }
  }

  emit nodeAdded(copy.id);

  return copy.id;
}

QString ChatTree::appendText(ChatNode::Kind kind, const QString &text,
                             const QString &parentId) {
  ChatNode node(kind, text);
  return append(node, parentId);
}

QString ChatTree::appendJob(ChatNode::Kind kind, const QString &title,
                            const QString &detail, const QString &jobId,
                            const QString &parentId) {
  ChatNode node(kind, title);
  node.detail = detail;
  node.jobId = jobId;
  node.state = ChatNode::State::Pending;
  return append(node, parentId);
}

ChatNode *ChatTree::node(const QString &id) {
  auto it = m_nodes.find(id);
  return it == m_nodes.end() ? nullptr : &it.value();
}

const ChatNode *ChatTree::node(const QString &id) const {
  auto it = m_nodes.constFind(id);
  return it == m_nodes.constEnd() ? nullptr : &it.value();
}

QVector<QString> ChatTree::childrenOf(const QString &id) const {
  const ChatNode *n = node(id);

  if (!n) {
    return {};
  }

  return n->children;
}

void ChatTree::setText(const QString &id, const QString &text) {
  auto it = m_nodes.find(id);

  if (it == m_nodes.end()) {
    return;
  }

  it->text = text;
  it->updatedAt = QDateTime::currentDateTime();

  emit nodeChanged(id);
}

void ChatTree::setDetail(const QString &id, const QString &detail) {
  auto it = m_nodes.find(id);

  if (it == m_nodes.end()) {
    return;
  }

  it->detail = detail;
  it->updatedAt = QDateTime::currentDateTime();

  emit nodeChanged(id);
}

void ChatTree::setState(const QString &id, ChatNode::State state) {
  auto it = m_nodes.find(id);

  if (it == m_nodes.end()) {
    return;
  }

  it->state = state;
  it->updatedAt = QDateTime::currentDateTime();

  emit nodeChanged(id);
}

void ChatTree::setResult(const QString &id, const QString &result) {
  auto it = m_nodes.find(id);

  if (it == m_nodes.end()) {
    return;
  }

  it->result = result;
  it->state = ChatNode::State::Done;
  it->updatedAt = QDateTime::currentDateTime();

  emit nodeChanged(id);
}

void ChatTree::setError(const QString &id, const QString &error) {
  auto it = m_nodes.find(id);

  if (it == m_nodes.end()) {
    return;
  }

  it->error = error;
  it->state = ChatNode::State::Failed;
  it->updatedAt = QDateTime::currentDateTime();

  emit nodeChanged(id);
}

void ChatTree::setCollapsed(const QString &id, bool collapsed) {
  auto it = m_nodes.find(id);

  if (it == m_nodes.end()) {
    return;
  }

  it->collapsed = collapsed;
  it->updatedAt = QDateTime::currentDateTime();

  emit nodeChanged(id);
}

void ChatTree::appendText(const QString &id, const QString &chunk) {
  auto it = m_nodes.find(id);

  if (it == m_nodes.end()) {
    return;
  }

  it->text += chunk;
  it->updatedAt = QDateTime::currentDateTime();

  emit nodeChanged(id);
}

QString ChatTree::jobIdFor(const QString &nodeId) const {
  const ChatNode *n = node(nodeId);
  return n ? n->jobId : QString();
}

QString ChatTree::nodeForJob(const QString &jobId) const {
  return m_jobToNode.value(jobId);
}

int ChatTree::inFlightCount() const {
  int count = 0;

  for (const ChatNode &node : m_nodes) {
    if (!node.isJob()) {
      continue;
    }

    if (node.state == ChatNode::State::Pending ||
        node.state == ChatNode::State::Running ||
        node.state == ChatNode::State::Waiting) {
      ++count;
    }
  }

  return count;
}