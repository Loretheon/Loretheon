#pragma once

#include <QWidget>

class MemoryPanel;
class OverviewPanel;
class OverseerSession;

class QTabWidget;
class QTextEdit;
class QScrollArea;
class QVBoxLayout;
class QLabel;

class OverseerSidePanel : public QWidget {
  Q_OBJECT

public:
  explicit OverseerSidePanel(QWidget *parent = nullptr);

  // Load the panels from a session. Passing nullptr clears them. Used
  // when the Overseer view switches sessions.
  void setSession(OverseerSession *session);

  QTabWidget *tabs() const { return m_tabs; }

  MemoryPanel *memoryPanel() const { return m_memoryPanel; }
  MemoryPanel *sessionMemoryPanel() const { return m_sessionMemoryPanel; }
  OverviewPanel *overviewPanel() const { return m_overviewPanel; }
  QTextEdit *toolLog() const { return m_toolLog; }

  QWidget *userActionsPage() const { return m_userActionsPage; }
  QScrollArea *userActionsScroll() const { return m_userActionsScroll; }
  QWidget *userActionsContent() const { return m_userActionsContent; }
  QVBoxLayout *userActionsLayout() const { return m_userActionsLayout; }
  QLabel *userActionsEmptyLabel() const { return m_userActionsEmptyLabel; }

private:
  QTabWidget *m_tabs = nullptr;
  MemoryPanel *m_memoryPanel = nullptr;
  MemoryPanel *m_sessionMemoryPanel = nullptr;
  OverviewPanel *m_overviewPanel = nullptr;
  QTextEdit *m_toolLog = nullptr;

  QWidget *m_userActionsPage = nullptr;
  QScrollArea *m_userActionsScroll = nullptr;
  QWidget *m_userActionsContent = nullptr;
  QVBoxLayout *m_userActionsLayout = nullptr;
  QLabel *m_userActionsEmptyLabel = nullptr;
};