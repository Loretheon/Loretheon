#pragma once

#include "OverseerRunner.h"

#include <QWidget>

class MemoryPanel;
class OverviewPanel;
class MemoryProposalCard;
class OverseerSession;

class QComboBox;
class QStackedWidget;
class QTextEdit;
class QScrollArea;
class QVBoxLayout;
class QLabel;

class OverseerSidePanel : public QWidget {
  Q_OBJECT

public:
  explicit OverseerSidePanel(QWidget *parent = nullptr);

  void setSession(OverseerSession *session);

  void setPendingActions(const QList<OverseerRunner::PendingAction> &actions);

  QComboBox *sectionPicker() const { return m_picker; }
  QStackedWidget *sectionStack() const { return m_stack; }

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
  void memoryProposalAccepted(const QString &key, const QString &scope);
  void memoryProposalRejected(const QString &key);

  void editPlanOpenRequested(const QString &planId);
  void editPlanApplyRequested(const QString &planId);
  void editPlanCancelRequested(const QString &planId);

private:
  void clearUserActionCards();
  void rebuildSectionPicker();

  QComboBox *m_picker = nullptr;
  QStackedWidget *m_stack = nullptr;

  MemoryPanel *m_memoryPanel = nullptr;
  MemoryPanel *m_sessionMemoryPanel = nullptr;
  OverviewPanel *m_overviewPanel = nullptr;
  QTextEdit *m_toolLog = nullptr;

  QWidget *m_userActionsPage = nullptr;
  QScrollArea *m_userActionsScroll = nullptr;
  QWidget *m_userActionsContent = nullptr;
  QVBoxLayout *m_userActionsLayout = nullptr;
  QLabel *m_userActionsEmptyLabel = nullptr;

  int m_userActionsIndex = -1;
  int m_userActionsCount = 0;
};