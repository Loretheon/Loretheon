#include "../../include/overseer/MemoryProposalCard.h"

#include <QComboBox>
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

  m_replacedLabel = new QLabel(this);
  m_replacedLabel->setWordWrap(true);
  m_replacedLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
  m_replacedLabel->setVisible(false);

  {
    QFont small = m_replacedLabel->font();
    small.setPointSizeF(qMax(7.0, small.pointSizeF() - 1.0));
    small.setItalic(true);
    m_replacedLabel->setFont(small);
  }

  m_rationaleLabel = new QLabel(rationale, this);
  m_rationaleLabel->setWordWrap(true);
  m_rationaleLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
  m_rationaleLabel->setVisible(!rationale.isEmpty());

  {
    QFont small = m_rationaleLabel->font();
    small.setPointSizeF(qMax(7.0, small.pointSizeF() - 1.0));
    m_rationaleLabel->setFont(small);
  }

  m_scopeCombo = new QComboBox(this);
  m_scopeCombo->addItem(tr("Global memory"), QStringLiteral("global"));
  m_scopeCombo->addItem(tr("Session memory"), QStringLiteral("session"));
  m_scopeCombo->setVisible(false);

  m_openButton = new QPushButton(tr("Open"), this);
  m_openButton->setToolTip(tr("Open the plan for review"));
  m_openButton->setVisible(false);

  m_acceptButton = new QPushButton(QStringLiteral("\u2713"), this);
  m_acceptButton->setToolTip(tr("Accept this proposal"));
  m_acceptButton->setFixedWidth(36);

  m_rejectButton = new QPushButton(QStringLiteral("\u2717"), this);
  m_rejectButton->setToolTip(tr("Reject this proposal"));
  m_rejectButton->setFixedWidth(36);

  auto *buttons = new QHBoxLayout;
  buttons->setContentsMargins(0, 0, 0, 0);
  buttons->setSpacing(6);
  buttons->addWidget(m_scopeCombo);
  buttons->addWidget(m_openButton);
  buttons->addWidget(m_acceptButton);
  buttons->addWidget(m_rejectButton);
  buttons->addStretch(1);

  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(10, 8, 10, 8);
  layout->setSpacing(4);
  layout->addWidget(m_factLabel);
  layout->addWidget(m_replacedLabel);
  layout->addWidget(m_rationaleLabel);
  layout->addLayout(buttons);

  connect(m_acceptButton, &QPushButton::clicked, this, [this]() {
    const QString scope = currentScope();

    setInteractive(false);
    emit accepted(m_key, scope);
  });

  connect(m_rejectButton, &QPushButton::clicked, this, [this]() {
    setInteractive(false);
    emit rejected(m_key);
  });

  connect(m_openButton, &QPushButton::clicked, this, [this]() {
    emit openRequested(m_key);
  });
}

void MemoryProposalCard::setMode(Mode mode, const QString &replacedFact) {
  m_mode = mode;

  if (!m_replacedLabel)
    return;

  switch (mode) {
  case Mode::NewFact:
    m_replacedLabel->setVisible(false);
    break;
  case Mode::Replace:
    m_replacedLabel->setText(
        tr("Replaces: %1").arg(replacedFact));
    m_replacedLabel->setVisible(!replacedFact.isEmpty());
    break;
  case Mode::Delete:
    m_replacedLabel->setText(
        tr("Deletes: %1").arg(replacedFact));
    m_replacedLabel->setVisible(!replacedFact.isEmpty());
    break;
  }
}

void MemoryProposalCard::setScopeSelectorVisible(bool visible,
                                                 const QString &currentScope) {
  if (!m_scopeCombo)
    return;

  if (visible) {
    const int index = m_scopeCombo->findData(currentScope);

    if (index >= 0)
      m_scopeCombo->setCurrentIndex(index);
  }

  m_scopeCombo->setVisible(visible);
}

void MemoryProposalCard::setOpenAffordanceVisible(bool visible) {
  if (m_openButton)
    m_openButton->setVisible(visible);
}

void MemoryProposalCard::setOpenAffordanceLabel(const QString &label) {
  if (m_openButton)
    m_openButton->setText(label);
}

void MemoryProposalCard::setOpenAffordanceTooltip(const QString &tooltip) {
  if (m_openButton)
    m_openButton->setToolTip(tooltip);
}

void MemoryProposalCard::setInteractive(bool interactive) {
  if (m_acceptButton)
    m_acceptButton->setEnabled(interactive);

  if (m_rejectButton)
    m_rejectButton->setEnabled(interactive);

  if (m_openButton)
    m_openButton->setEnabled(interactive);

  if (m_scopeCombo)
    m_scopeCombo->setEnabled(interactive);
}

QString MemoryProposalCard::currentScope() const {
  if (!m_scopeCombo || !m_scopeCombo->isVisible())
    return QStringLiteral("global");

  const QString value = m_scopeCombo->currentData().toString();

  return value.isEmpty() ? QStringLiteral("global") : value;
}