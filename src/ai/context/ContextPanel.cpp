#include "../../../include/ai/context/ContextPanel.h"

#include <QBrush>
#include <QCheckBox>
#include <QColor>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QTimer>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

ContextPanel::ContextPanel(ContextModel *model, QWidget *parent)
    : QWidget(parent), m_model(model) {
  auto *rootLayout = new QVBoxLayout(this);

  rootLayout->setContentsMargins(8, 8, 8, 8);
  rootLayout->setSpacing(6);

  auto *headerRow = new QHBoxLayout;

  m_summary = new QLabel(this);
  m_summary->setWordWrap(true);

  headerRow->addWidget(m_summary, 1);

  rootLayout->addLayout(headerRow);

  auto *buttonRow = new QHBoxLayout;

  m_includeAllButton = new QPushButton(tr("Include All"), this);
  m_excludeAllButton = new QPushButton(tr("Exclude All"), this);
  m_refreshButton = new QPushButton(tr("Refresh"), this);

  buttonRow->addWidget(m_includeAllButton);
  buttonRow->addWidget(m_excludeAllButton);
  buttonRow->addStretch();
  buttonRow->addWidget(m_refreshButton);

  rootLayout->addLayout(buttonRow);

  m_tree = new QTreeWidget(this);
  m_tree->setColumnCount(3);
  m_tree->setHeaderLabels({tr("Scope"), tr("Status"), tr("Included")});
  m_tree->setRootIsDecorated(false);
  m_tree->setUniformRowHeights(true);
  m_tree->setSelectionMode(QAbstractItemView::NoSelection);
  m_tree->setColumnWidth(0, 220);
  m_tree->setColumnWidth(1, 90);

  rootLayout->addWidget(m_tree, 1);

  if (m_model) {
    connect(m_model, &ContextModel::changed, this, &ContextPanel::rebuild);
  }

  connect(m_includeAllButton, &QPushButton::clicked, this, [this] {
    if (m_model) {
      m_model->setAllIncluded(true);
    }
  });

  connect(m_excludeAllButton, &QPushButton::clicked, this, [this] {
    if (m_model) {
      m_model->setAllIncluded(false);
    }
  });

  connect(m_refreshButton, &QPushButton::clicked, this, [this] {
    rebuild();
  });

  connect(m_tree, &QTreeWidget::itemChanged, this,
          &ContextPanel::onItemChanged);

  rebuild();
}

void ContextPanel::rebuild() {
  if (!m_model || !m_tree) {
    return;
  }

  m_rebuilding = true;

  {
    QSignalBlocker blocker(m_tree);

    m_tree->clear();

    const QVector<ContextModel::Entry> &entries = m_model->entries();

    int includedCount = 0;
    int staleCount = 0;

    for (const ContextModel::Entry &entry : entries) {
      if (entry.included) {
        ++includedCount;
      }

      if (entry.isStale()) {
        ++staleCount;
      }

      auto *item = new QTreeWidgetItem(m_tree);

      const QString label =
          entry.heading.isEmpty() ? entry.scopeId : entry.heading;

      item->setText(0, label);
      item->setToolTip(0, entry.scopeId);

      QString status;

      if (entry.isStale()) {
        status = tr("Changed");
      } else if (entry.isNew()) {
        status = tr("New");
      } else if (entry.isSent()) {
        status = tr("Sent");
      } else {
        status = QStringLiteral("—");
      }

      item->setText(1, status);
      item->setData(1, Qt::UserRole, entry.scopeId);

      item->setCheckState(2, entry.included ? Qt::Checked : Qt::Unchecked);

      if (entry.isStale()) {
        item->setForeground(1, QBrush(QColor(210, 120, 60)));
      } else if (entry.isNew()) {
        item->setForeground(1, QBrush(QColor(80, 160, 90)));
      }
    }

    m_summary->setText(
        tr("%1 scopes · %2 included · %3 changed since last send")
            .arg(entries.size())
            .arg(includedCount)
            .arg(staleCount));
  }

  m_rebuilding = false;
}

void ContextPanel::onItemChanged(QTreeWidgetItem *item, int column) {
  if (m_rebuilding || !m_model || !item || column != 2) {
    return;
  }

  const QString scopeId = item->data(1, Qt::UserRole).toString();

  if (scopeId.isEmpty()) {
    return;
  }

  const bool included = item->checkState(2) == Qt::Checked;

  // Defer the model mutation so we are not inside the tree's own signal
  // emission when rebuild() runs.
  QTimer::singleShot(0, this, [this, scopeId, included] {
    if (m_model) {
      m_model->setIncluded(scopeId, included);
    }
  });
}