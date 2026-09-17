#include "../../include/overseer/MemoryFactCard.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QVBoxLayout>

MemoryFactCard::MemoryFactCard(const QString &fact, const QString &rationale,
                               QWidget *parent)
    : CardWidget(parent), m_fact(fact), m_rationale(rationale) {
  setTitle(fact);
  setCloseVisible(true);
  buildBody();
}

void MemoryFactCard::setFact(const QString &fact) {
  m_fact = fact;
  setTitle(fact);

  if (m_factEdit)
    m_factEdit->setText(fact);
}

void MemoryFactCard::setRationale(const QString &rationale) {
  m_rationale = rationale;

  if (m_rationaleLabel) {
    m_rationaleLabel->setText(rationale);
    m_rationaleLabel->setVisible(!rationale.isEmpty());
  }
}

void MemoryFactCard::populateBody(QVBoxLayout *bodyLayout) {
  m_rationaleLabel = new QLabel(m_rationale, this);
  m_rationaleLabel->setWordWrap(true);
  m_rationaleLabel->setObjectName(QStringLiteral("memoryFactRationale"));
  m_rationaleLabel->setVisible(!m_rationale.isEmpty());

  m_factEdit = new QLineEdit(m_fact, this);
  m_factEdit->setObjectName(QStringLiteral("memoryFactEdit"));
  m_factEdit->hide();

  bodyLayout->addWidget(m_rationaleLabel);
  bodyLayout->addWidget(m_factEdit);

  connect(m_factEdit, &QLineEdit::returnPressed, this,
          &MemoryFactCard::commitEdit);

  connect(this, &CardWidget::closeRequested, this,
          &MemoryFactCard::removed);

  connect(this, &CardWidget::clicked, this, [this]() {
    if (!m_editing)
      beginInlineEdit();
  });
}

void MemoryFactCard::beginInlineEdit() {
  if (m_editing)
    return;

  m_editing = true;
  m_factEdit->setText(m_fact);
  m_factEdit->show();
  m_factEdit->setFocus();
  m_factEdit->selectAll();
}

void MemoryFactCard::commitEdit() {
  if (!m_editing)
    return;

  const QString value = m_factEdit->text().trimmed();

  m_editing = false;
  m_factEdit->hide();

  if (value.isEmpty() || value == m_fact)
    return;

  m_fact = value;
  setTitle(m_fact);

  emit edited(m_fact);
}

void MemoryFactCard::cancelEdit() {
  m_editing = false;
  m_factEdit->hide();
}

void MemoryFactCard::refreshBodyVisibility() {
  if (m_rationaleLabel)
    m_rationaleLabel->setVisible(!m_rationale.isEmpty());
}

QMenu *MemoryFactCard::buildContextMenu(QWidget *parent) {
  auto *menu = new QMenu(parent);

  QAction *edit = menu->addAction(tr("Edit"));
  QAction *del = menu->addAction(tr("Delete"));
  QAction *copy = menu->addAction(tr("Copy text"));

  menu->addSeparator();

  QAction *moveTop = menu->addAction(tr("Move to top"));
  QAction *moveBottom = menu->addAction(tr("Move to bottom"));

  connect(edit, &QAction::triggered, this, &MemoryFactCard::beginInlineEdit);

  connect(del, &QAction::triggered, this, &MemoryFactCard::removed);

  connect(copy, &QAction::triggered, this, [this]() {
    QGuiApplication::clipboard()->setText(m_fact);
  });

  connect(moveTop, &QAction::triggered, this,
          &MemoryFactCard::moveToTopRequested);

  connect(moveBottom, &QAction::triggered, this,
          &MemoryFactCard::moveToBottomRequested);

  return menu;
}