#include "../../include/overseer/MemoryProposalCard.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

MemoryProposalCard::MemoryProposalCard(const QString &key,
                                       const QString &fact,
                                       const QString &rationale,
                                       QWidget *parent)
    : QWidget(parent), m_key(key) {
  setObjectName(QStringLiteral("memoryProposalCard"));

  m_factLabel = new QLabel(fact, this);
  m_factLabel->setWordWrap(true);
  m_factLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

  m_rationaleLabel = new QLabel(rationale, this);
  m_rationaleLabel->setWordWrap(true);
  m_rationaleLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
  m_rationaleLabel->setVisible(!rationale.isEmpty());

  {
    QFont small = m_rationaleLabel->font();
    small.setPointSizeF(qMax(7.0, small.pointSizeF() - 1.0));
    m_rationaleLabel->setFont(small);
  }

  m_acceptButton = new QPushButton(QStringLiteral("\u2713"), this);
  m_acceptButton->setToolTip(tr("Accept this fact into memory"));
  m_acceptButton->setFixedWidth(36);

  m_rejectButton = new QPushButton(QStringLiteral("\u2717"), this);
  m_rejectButton->setToolTip(tr("Reject this proposal"));
  m_rejectButton->setFixedWidth(36);

  auto *buttons = new QHBoxLayout;
  buttons->setContentsMargins(0, 0, 0, 0);
  buttons->setSpacing(6);
  buttons->addWidget(m_acceptButton);
  buttons->addWidget(m_rejectButton);
  buttons->addStretch(1);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(10, 8, 10, 8);
  layout->setSpacing(4);
  layout->addWidget(m_factLabel);
  layout->addWidget(m_rationaleLabel);
  layout->addLayout(buttons);

  connect(m_acceptButton, &QPushButton::clicked, this, [this]() {
    m_acceptButton->setEnabled(false);
    m_rejectButton->setEnabled(false);
    emit accepted(m_key);
  });

  connect(m_rejectButton, &QPushButton::clicked, this, [this]() {
    m_acceptButton->setEnabled(false);
    m_rejectButton->setEnabled(false);
    emit rejected(m_key);
  });
}