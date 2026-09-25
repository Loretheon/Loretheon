#pragma once

#include <QGraphicsScene>
#include <QHash>
#include <QString>

class MindMapNode;

class MindMapScene : public QGraphicsScene {
  Q_OBJECT

public:
  explicit MindMapScene(QObject *parent = nullptr);

  void build(const QString &assistantRoot);

  MindMapNode *root() const { return m_root; }
  MindMapNode *nodeFor(const QString &path) const;

  void focusOn(MindMapNode *node);
  void clearFocus();

  void setVisibleDepth(int depth);
  int visibleDepth() const { return m_visibleDepth; }
  void layoutTree();

  signals:
    void nodeHovered(MindMapNode *node);
  void contentChanged();

private:
  MindMapNode *addProfileFile(const QString &title, const QString &path,
                              const QString &body);
  MindMapNode *addMemoryFile(const QString &path);
  void addToScene(MindMapNode *node);

  MindMapNode *m_root = nullptr;
  int m_visibleDepth = 3;

  QHash<QString, MindMapNode *> m_nodesByPath;
};