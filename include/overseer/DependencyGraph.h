#pragma once

#include <QHash>
#include <QSet>
#include <QString>
#include <QStringList>
#include <QVector>

// A simple dependency graph backed by a DOT file. Nodes are task ids.
// An edge from A to B means B must complete before A starts.
//
// The file is human-editable. The parser accepts a minimal subset of
// DOT: "digraph { a -> b; c -> d; }" with optional comments.
class DependencyGraph {
public:
  void setPath(const QString &path);
  QString path() const { return m_path; }

  void load();
  void save() const;

  void addNode(const QString &taskId);
  void addEdge(const QString &from, const QString &to);
  void removeNode(const QString &taskId);
  void removeEdge(const QString &from, const QString &to);

  bool contains(const QString &taskId) const;
  QStringList nodes() const;
  QStringList edgesFrom(const QString &taskId) const;
  QStringList edgesTo(const QString &taskId) const;

  // Tasks with no unfinished dependencies that are not in doneIds.
  // Returned in insertion order.
  QStringList ready(const QSet<QString> &doneIds) const;

  bool hasCycle() const;

private:
  QString m_path;
  QStringList m_nodeOrder;
  QSet<QString> m_nodes;
  QVector<QPair<QString, QString>> m_edges;
};