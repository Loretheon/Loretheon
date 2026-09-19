#include "../../include/overseer/DependencyGraph.h"

#include <QFile>
#include <QRegularExpression>
#include <QSet>
#include <QTextStream>

void DependencyGraph::setPath(const QString &path) {
  m_path = path;
  load();
}

void DependencyGraph::load() {
  m_nodeOrder.clear();
  m_nodes.clear();
  m_edges.clear();

  if (m_path.isEmpty())
    return;

  QFile file(m_path);

  if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    return;

  QTextStream stream(&file);
  stream.setEncoding(QStringConverter::Utf8);

  const QString text = stream.readAll();

  static const QRegularExpression edgeRe(
      QStringLiteral(R"(([A-Za-z0-9_\-]+)\s*->\s*([A-Za-z0-9_\-]+))"));

  auto it = edgeRe.globalMatch(text);

  while (it.hasNext()) {
    const auto match = it.next();

    const QString from = match.captured(1);
    const QString to = match.captured(2);

    if (from == QStringLiteral("digraph") || to == QStringLiteral("digraph"))
      continue;

    if (!m_nodes.contains(from)) {
      m_nodes.insert(from);
      m_nodeOrder.append(from);
    }

    if (!m_nodes.contains(to)) {
      m_nodes.insert(to);
      m_nodeOrder.append(to);
    }

    m_edges.append({from, to});
  }
}

void DependencyGraph::save() const {
  if (m_path.isEmpty())
    return;

  QFile file(m_path);

  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate |
                 QIODevice::Text))
    return;

  QTextStream stream(&file);
  stream.setEncoding(QStringConverter::Utf8);

  stream << "digraph dependencies {\n";

  for (const auto &edge : m_edges)
    stream << "  " << edge.first << " -> " << edge.second << ";\n";

  stream << "}\n";
}

void DependencyGraph::addNode(const QString &taskId) {
  if (taskId.isEmpty() || m_nodes.contains(taskId))
    return;

  m_nodes.insert(taskId);
  m_nodeOrder.append(taskId);
  save();
}

void DependencyGraph::addEdge(const QString &from, const QString &to) {
  if (from.isEmpty() || to.isEmpty())
    return;

  if (from == to)
    return;

  addNode(from);
  addNode(to);

  for (const auto &edge : std::as_const(m_edges)) {
    if (edge.first == from && edge.second == to)
      return;
  }

  m_edges.append({from, to});
  save();
}

void DependencyGraph::removeNode(const QString &taskId) {
  if (!m_nodes.remove(taskId))
    return;

  m_nodeOrder.removeAll(taskId);

  for (int i = m_edges.size() - 1; i >= 0; --i) {
    if (m_edges.at(i).first == taskId || m_edges.at(i).second == taskId)
      m_edges.removeAt(i);
  }

  save();
}

void DependencyGraph::removeEdge(const QString &from, const QString &to) {
  for (int i = 0; i < m_edges.size(); ++i) {
    if (m_edges.at(i).first != from || m_edges.at(i).second != to)
      continue;

    m_edges.removeAt(i);
    save();
    return;
  }
}

bool DependencyGraph::contains(const QString &taskId) const {
  return m_nodes.contains(taskId);
}

QStringList DependencyGraph::nodes() const { return m_nodeOrder; }

QStringList DependencyGraph::edgesFrom(const QString &taskId) const {
  QStringList result;

  for (const auto &edge : m_edges) {
    if (edge.first == taskId)
      result.append(edge.second);
  }

  return result;
}

QStringList DependencyGraph::edgesTo(const QString &taskId) const {
  QStringList result;

  for (const auto &edge : m_edges) {
    if (edge.second == taskId)
      result.append(edge.first);
  }

  return result;
}

QStringList DependencyGraph::ready(const QSet<QString> &doneIds) const {
  QStringList result;

  for (const QString &node : m_nodeOrder) {
    if (doneIds.contains(node))
      continue;

    const QStringList deps = edgesTo(node);

    bool allDone = true;

    for (const QString &dep : deps) {
      if (!doneIds.contains(dep)) {
        allDone = false;
        break;
      }
    }

    if (allDone)
      result.append(node);
  }

  return result;
}

bool DependencyGraph::hasCycle() const {
  QSet<QString> visited;
  QSet<QString> stack;

  std::function<bool(const QString &)> visit = [&](const QString &node) {
    if (stack.contains(node))
      return true;

    if (visited.contains(node))
      return false;

    visited.insert(node);
    stack.insert(node);

    for (const QString &dep : edgesFrom(node)) {
      if (visit(dep))
        return true;
    }

    stack.remove(node);
    return false;
  };

  for (const QString &node : m_nodeOrder) {
    if (visit(node))
      return true;
  }

  return false;
}