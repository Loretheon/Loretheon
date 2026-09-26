#include "../../include/overseer/OverseerSidePanel.h"

#include "MemoryPanel.h"
#include "MemoryProposalCard.h"
#include "OverseerSession.h"
#include "OverseerStorage.h"
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

void OverseerSidePanel::setSession(OverseerSession *session) {
  if (!m_memoryPanel || !m_sessionMemoryPanel || !m_overviewPanel)
    return;

  m_memoryPanel->loadFromFile(OverseerStorage::memoryPath());

  if (session) {
    m_sessionMemoryPanel->loadFromFile(session->memoryPath());
    m_overviewPanel->loadFromFile(session->overviewPath());
  }
}

void OverseerSidePanel::clearUserActionCards() {
  if (!m_userActionsContent || !m_userActionsLayout)
    return;

  const QList<QWidget *> children =
      m_userActionsContent->findChildren<QWidget *>(
          QString(), Qt::FindDirectChildrenOnly);

  for (QWidget *w : children) {
    if (w == m_userActionsEmptyLabel)
      continue;

    m_userActionsLayout->removeWidget(w);
    w->deleteLater();
  }
}

void OverseerSidePanel::setPendingActions(
    const QList<OverseerRunner::PendingAction> &actions) {
  if (!m_userActionsContent || !m_userActionsLayout)
    return;

  clearUserActionCards();

  int inserted = 0;

  for (const OverseerRunner::PendingAction &action : actions) {
    auto *card = new MemoryProposalCard(action.key, action.title,
                                        action.subtitle,
                                        m_userActionsContent);

    switch (action.kind) {
    case OverseerRunner::PendingAction::Kind::MemoryProposal: {
      switch (action.proposalMode) {
      case OverseerRunner::PendingAction::ProposalMode::NewFact:
        card->setMode(MemoryProposalCard::Mode::NewFact, QString());
        break;
      case OverseerRunner::PendingAction::ProposalMode::Replace:
        card->setMode(MemoryProposalCard::Mode::Replace,
                      action.replacedFact);
        break;
      case OverseerRunner::PendingAction::ProposalMode::Delete:
        card->setMode(MemoryProposalCard::Mode::Delete,
                      action.replacedFact);
        break;
      }

      // The scope combo only appears for NewFact and Replace. A
      // deletion has a fixed scope, chosen by the proposal.
      const bool showScope =
          action.proposalMode !=
          OverseerRunner::PendingAction::ProposalMode::Delete;

      card->setScopeSelectorVisible(showScope, action.scope);
      card->setOpenAffordanceVisible(false);

      connect(card, &MemoryProposalCard::accepted, this,
              &OverseerSidePanel::memoryProposalAccepted);

      connect(card, &MemoryProposalCard::rejected, this,
              &OverseerSidePanel::memoryProposalRejected);

      break;
    }
    case OverseerRunner::PendingAction::Kind::EditPlan: {
      card->setScopeSelectorVisible(false, QString());
      card->setOpenAffordanceVisible(true);
      card->setOpenAffordanceLabel(tr("Open"));
      card->setOpenAffordanceTooltip(
          tr("Open the plan for review in the transcript"));

      connect(card, &MemoryProposalCard::accepted, this,
              [this](const QString &key, const QString &) {
                emit editPlanApplyRequested(key);
              });

      connect(card, &MemoryProposalCard::rejected, this,
              [this](const QString &key) {
                emit editPlanCancelRequested(key);
              });

      connect(card, &MemoryProposalCard::openRequested, this,
              [this](const QString &key) {
                emit editPlanOpenRequested(key);
              });

      break;
    }
    }

    m_userActionsLayout->insertWidget(inserted, card);
    ++inserted;
  }

  m_userActionsEmptyLabel->setVisible(inserted == 0);
}