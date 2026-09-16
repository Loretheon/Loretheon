#include "../../include/overseer/OverseerSidePanel.h"

#include "../../include/overseer/MemoryPanel.h"
#include "../../include/overseer/OverviewPanel.h"

#include <QTabWidget>
#include <QVBoxLayout>

OverseerSidePanel::OverseerSidePanel(QWidget *parent) : QWidget(parent) {
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(0, 0, 0, 0);

  m_tabs = new QTabWidget(this);

  m_memoryPanel = new MemoryPanel(m_tabs);
  m_overviewPanel = new OverviewPanel(m_tabs);

  m_tabs->addTab(m_memoryPanel, tr("Memory"));
  m_tabs->addTab(m_overviewPanel, tr("Overview"));

  root->addWidget(m_tabs);
}