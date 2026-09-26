#pragma once

#include "OverseerRunner.h"

#include <QWidget>

class MemoryPanel;
class OverviewPanel;
class MemoryProposalCard;
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

  // Populate the "User actions" tab with the current pending actions.
  // Clears and rebuilds the content each call. Passing an empty list
  // shows the empty-state label.
  void setPendingActions(const QList<OverseerRunner::PendingAction> &actions);

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

signals:
  // Forwarded from the memory proposal cards built into the user
  // actions tab. OverseerWidget wires these to the same slots the
  // transcript panel's signals already use, so both views drive the
  // same runner code path.
  void memoryProposalAccepted(const QString &key, const QString &scope);
  void memoryProposalRejected(const QString &key);

  // Edit plan actions.
  void editPlanOpenRequested(const QString &planId);
  void editPlanApplyRequested(const QString &planId);
  void editPlanCancelRequested(const QString &planId);

private:
  void clearUserActionCards();

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