#pragma once

#include <QGraphicsScene>
#include <QHash>
#include <QPointF>
#include <QString>
#include <QTimer>

class MindMapNode;
class OverseerSessionManager;

class MindMapScene : public QGraphicsScene {
  Q_OBJECT

public:
  explicit MindMapScene(QObject *parent = nullptr);
  ~MindMapScene() override;

  void setOverseerManager(OverseerSessionManager *manager);
  OverseerSessionManager *overseerManager() const { return m_overseer; }

  void setAssistantRoot(const QString &root);
  QString assistantRoot() const { return m_assistantRoot; }

  void build();
  void refresh();
  void rebuildLayout();
  void saveLayout() const;

  MindMapNode *root() const { return m_root; }

  void focusOn(MindMapNode *node);
  void clearFocus();

  void pinNode(MindMapNode *node, const QPointF &pos);

  void openNode(MindMapNode *node);

signals:
  void openRequested(const QString &path);
  void sessionOpenRequested(const QString &sessionName);
  void contentChanged();

protected:
  void mousePressEvent(QGraphicsSceneMouseEvent *event) override;
  void drawForeground(QPainter *painter, const QRectF &rect) override;

private slots:
  void onSimulationTick();

private:
  MindMapNode *addProfileFile(const QString &title, const QString &path,
                              const QString &body);
  MindMapNode *addMemoryTopic(const QString &path);
  MindMapNode *addMemorySession(const QString &path);
  MindMapNode *addOverseerSession(const QString &name,
                                  const QString &description);

  void addToScene(MindMapNode *node);

  void collectAllNodes();
  void collectTreeEdges();
  void buildCrossLinks();

  void seedPositions();
  void loadLayoutFromDisk();
  bool hasCache() const;

  void startSimulation(int maxTicks);

  QString cachePath() const;

  QString m_assistantRoot;

  MindMapNode *m_root = nullptr;

  OverseerSessionManager *m_overseer = nullptr;

  QHash<QString, MindMapNode *> m_nodesById;
  QHash<QString, MindMapNode *> m_nodesByPath;

  QList<MindMapNode *> m_allNodes;

  QVector<QPair<MindMapNode *, MindMapNode *>> m_edges;

  QTimer *m_simTimer = nullptr;
  int m_simTicksLeft = 0;
  int m_simIteration = 0;
};