#include "../../include/overseer/OverseerSessionList.h"

#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QVBoxLayout>

OverseerSessionList::OverseerSessionList(QWidget *parent) : QWidget(parent) {
  auto *layout = new QVBoxLayout(this);
  layout->setContentsMargins(6, 6, 6, 6);
  layout->setSpacing(6);

  auto *label = new QLabel(tr("Sessions"), this);
  QFont f = label->font();
  f.setBold(true);
  label->setFont(f);
  layout->addWidget(label);

  m_list = new QListWidget(this);
  m_list->setObjectName(QStringLiteral("overseerSessionList"));
  layout->addWidget(m_list, 1);

  m_newButton = new QPushButton(tr("New Session"), this);
  layout->addWidget(m_newButton);

  connect(m_list, &QListWidget::itemSelectionChanged, this,
          &OverseerSessionList::onSelectionChanged);

  connect(m_newButton, &QPushButton::clicked, this,
          &OverseerSessionList::newSessionRequested);
}

void OverseerSessionList::rebuild(const QStringList &names) {
  const QString current = m_list->currentItem() ? m_list->currentItem()->text()
                                                : QString();

  QSignalBlocker blocker(m_list);

  m_list->clear();

  for (const QString &name : names) {
    m_list->addItem(name);
  }

  if (!current.isEmpty()) {
    const auto items = m_list->findItems(current, Qt::MatchExactly);
    if (!items.isEmpty()) {
      m_list->setCurrentItem(items.first());
    }
  }
}

void OverseerSessionList::selectByName(const QString &name) {
  const auto items = m_list->findItems(name, Qt::MatchExactly);
  if (!items.isEmpty()) {
    m_list->setCurrentItem(items.first());
  }
}

void OverseerSessionList::onSelectionChanged() {
  const auto selected = m_list->selectedItems();

  if (selected.isEmpty()) {
    emit sessionCleared();
    return;
  }

  emit sessionSelected(selected.first()->text());
}