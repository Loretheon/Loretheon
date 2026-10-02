#pragma once

#include <QWidget>

class ConductorQueue;
class ConductorRoster;
class DependencyGraph;
class DiagramDocument;
class DiagramToolbar;
class DiagramView;
class GraphvizRenderer;
class QScrollArea;
class QStackedWidget;
class QTabWidget;
class QVBoxLayout;

class ConductorBoard : public QWidget {
  Q_OBJECT

public:
  explicit ConductorBoard(QWidget *parent = nullptr);

  void setQueue(ConductorQueue *queue);
  void setRoster(ConductorRoster *roster);
  void setDependencies(DependencyGraph *graph);

  void setSessionFolder(const QString &folder);

  signals:
    void cancelRequested(const QString &requestId);
  void removeRequested(const QString &requestId);
  void retryRequested(const QString &requestId);
  void skipRequested(const QString &requestId);

protected:
  void changeEvent(QEvent *event) override;

private slots:
  void rebuild();
  void showDetail(const QString &requestId);
  void popDetail();

private:
  QWidget *buildKanbanTab();
  QWidget *buildGraphTab();
  QWidget *buildDetailPanel(const QString &requestId);

  void renderDependencyGraph();
  QString buildDependencyDot() const;
  QString nodeLabelFor(const QString &requestId) const;
  QString workerLabelFor(const QString &workerId) const;

  ConductorQueue *m_queue = nullptr;
  ConductorRoster *m_roster = nullptr;
  DependencyGraph *m_dependencies = nullptr;

  QString m_sessionFolder;

  QStackedWidget *m_stack = nullptr;
  QWidget *m_conductorBoard = nullptr;

  QTabWidget *m_tabs = nullptr;

  QScrollArea *m_kanbanScroll = nullptr;

  QWidget *m_graphPanel = nullptr;
  DiagramView *m_graphView = nullptr;
  DiagramToolbar *m_graphToolbar = nullptr;
  DiagramDocument *m_graphDocument = nullptr;
  GraphvizRenderer *m_graphRenderer = nullptr;

  QString m_detailRequestId;
};