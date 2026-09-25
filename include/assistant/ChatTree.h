#pragma once

#include "ChatNode.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

class ChatTree : public QObject {
  Q_OBJECT

public:
  explicit ChatTree(QObject *parent = nullptr);

  void clear();

  QString append(const ChatNode &node, const QString &parentId);

  // Append with a caller-chosen id. Returns the id, or an empty string
  // if the id was empty or already present.
  QString appendWithId(const ChatNode &node, const QString &id,
                       const QString &parentId);

  QString appendText(ChatNode::Kind kind, const QString &text,
                     const QString &parentId);

  QString appendJob(ChatNode::Kind kind, const QString &title,
                    const QString &detail, const QString &jobId,
                    const QString &parentId);

  ChatNode *node(const QString &id);
  const ChatNode *node(const QString &id) const;

  QVector<QString> roots() const { return m_roots; }
  QVector<QString> childrenOf(const QString &id) const;

  void setText(const QString &id, const QString &text);
  void setDetail(const QString &id, const QString &detail);
  void setState(const QString &id, ChatNode::State state);
  void setResult(const QString &id, const QString &result);
  void setError(const QString &id, const QString &error);
  void setCollapsed(const QString &id, bool collapsed);

  void appendText(const QString &id, const QString &chunk);

  QString jobIdFor(const QString &nodeId) const;
  QString nodeForJob(const QString &jobId) const;

  int inFlightCount() const;

  signals:
    void nodeAdded(const QString &id);
  void nodeChanged(const QString &id);
  void cleared();

private:
  QHash<QString, ChatNode> m_nodes;
  QHash<QString, QString> m_jobToNode;

  QVector<QString> m_roots;

  quint64 m_nextOrdinal = 1;
};