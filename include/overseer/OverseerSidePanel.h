#pragma once

#include <QWidget>

class MemoryPanel;
class OverviewPanel;

class QTabWidget;

class OverseerSidePanel : public QWidget {
  Q_OBJECT

public:
  explicit OverseerSidePanel(QWidget *parent = nullptr);

  QTabWidget *tabs() const { return m_tabs; }

  MemoryPanel *memoryPanel() const { return m_memoryPanel; }
  OverviewPanel *overviewPanel() const { return m_overviewPanel; }

private:
  QTabWidget *m_tabs = nullptr;
  MemoryPanel *m_memoryPanel = nullptr;
  OverviewPanel *m_overviewPanel = nullptr;
};