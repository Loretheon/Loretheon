#include "../../include/overseer/OverseerSidePanel.h"

#include "MemoryPanel.h"
#include "OverviewPanel.h"

#include <QFontDatabase>
#include <QLabel>
#include <QScrollArea>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>

OverseerSidePanel::OverseerSidePanel(QWidget *parent) : QWidget(parent) {
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);

  m_tabs = new QTabWidget(this);

  m_memoryPanel = new MemoryPanel(m_tabs);
  m_sessionMemoryPanel = new MemoryPanel(m_tabs);
  m_overviewPanel = new OverviewPanel(m_tabs);

  m_toolLog = new QTextEdit(m_tabs);
  m_toolLog->setReadOnly(true);
  m_toolLog->setAcceptRichText(false);
  m_toolLog->setLineWrapMode(QTextEdit::NoWrap);
  m_toolLog->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
  m_toolLog->setObjectName(QStringLiteral("overseerToolLog"));

  m_userActionsPage = new QWidget(m_tabs);
  auto *userActionsLayout = new QVBoxLayout(m_userActionsPage);
  userActionsLayout->setContentsMargins(6, 6, 6, 6);
  userActionsLayout->setSpacing(6);

  m_userActionsScroll = new QScrollArea(m_userActionsPage);
  m_userActionsScroll->setWidgetResizable(true);
  m_userActionsScroll->setFrameShape(QFrame::NoFrame);

  m_userActionsContent = new QWidget;
  m_userActionsLayout = new QVBoxLayout(m_userActionsContent);
  m_userActionsLayout->setContentsMargins(0, 0, 0, 0);
  m_userActionsLayout->setSpacing(8);
  m_userActionsLayout->setAlignment(Qt::AlignTop);

  m_userActionsEmptyLabel =
      new QLabel(tr("Nothing needs your attention."), m_userActionsContent);
  m_userActionsEmptyLabel->setAlignment(Qt::AlignCenter);

  m_userActionsLayout->addWidget(m_userActionsEmptyLabel);
  m_userActionsLayout->addStretch(1);

  m_userActionsScroll->setWidget(m_userActionsContent);

  userActionsLayout->addWidget(m_userActionsScroll, 1);

  m_tabs->addTab(m_memoryPanel, tr("Memory (global)"));
  m_tabs->addTab(m_sessionMemoryPanel, tr("Memory (session)"));
  m_tabs->addTab(m_overviewPanel, tr("Overview"));
  m_tabs->addTab(m_toolLog, tr("Tools"));
  m_tabs->addTab(m_userActionsPage, tr("User actions"));

  root->addWidget(m_tabs);
}