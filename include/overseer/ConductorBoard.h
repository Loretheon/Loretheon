#pragma once

#include <QWidget>

class ConductorQueue;
class ConductorRoster;
class DependencyGraph;
class DiagramDocument;
class DiagramToolbar;
class DiagramView;
class GraphvizRenderer;
class QLabel;
class QSplitter;
class QStackedWidget;
class QVBoxLayout;

class ConductorBoard : public QWidget {
  Q_OBJECT

public:
  explicit ConductorBoard(QWidget *parent = nullptr);

  void setQueue(ConductorQueue *queue);
  void setRoster(ConductorRoster *roster);
  void setDependencies(DependencyGraph *graph);

  // The session folder used to persist splitter geometry and the
  // generated kanban export. Empty means do not persist.
  void setSessionFolder(const QString &folder);

signals:
  void cancelRequested(const QString &requestId);
  void removeRequested(const QString &requestId);

protected:
  void changeEvent(QEvent *event) override;

private slots:
  void rebuild();
  void showDetail(const QString &requestId);
  void popDetail();

private:
  QWidget *buildConductorBoard();
  QWidget *buildGraphPanel();
  QWidget *buildDetailPanel(const QString &requestId);
  void rebuildGraphView();
  void rebuildRosterView();

  void renderDependencyGraph();
  QString buildDependencyDot() const;
  QString nodeLabelFor(const QString &requestId) const;

  void loadSplitterState();
  void saveSplitterState() const;
  QString splitterStatePath() const;

  ConductorQueue *m_queue = nullptr;
  ConductorRoster *m_roster = nullptr;
  DependencyGraph *m_dependencies = nullptr;

  QString m_sessionFolder;

  QStackedWidget *m_stack = nullptr;
  QWidget *m_conductorBoard = nullptr;

  // Vertical splitter between the dependency graph (top) and the
  // kanban scroll area (bottom).
  QSplitter *m_boardSplitter = nullptr;

  QLabel *m_graphLabel = nullptr;
  QLabel *m_rosterLabel = nullptr;

  QWidget *m_graphPanel = nullptr;
  DiagramView *m_graphView = nullptr;
  DiagramToolbar *m_graphToolbar = nullptr;
  DiagramDocument *m_graphDocument = nullptr;
  GraphvizRenderer *m_graphRenderer = nullptr;

  QString m_detailRequestId;
};