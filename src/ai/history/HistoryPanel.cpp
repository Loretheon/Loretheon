#include "../../../include/ai/history/HistoryPanel.h"

#include <QDateTime>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QVBoxLayout>

namespace {

QString operationLabel(const EditCommand &command) {
  switch (command.operation) {
  case EditCommand::Operation::Insert:
    return QObject::tr("Insert");

  case EditCommand::Operation::Replace:
    return QObject::tr("Replace");

  case EditCommand::Operation::ReplaceScope:
    return QObject::tr("Replace Scope");

  case EditCommand::Operation::Delete:
    return QObject::tr("Delete");

  case EditCommand::Operation::Unknown:
    return QObject::tr("Unknown");
  }

  return {};
}

} // namespace

HistoryPanel::HistoryPanel(HistoryModel *model, QWidget *parent)
    : QWidget(parent), m_model(model) {
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(8, 8, 8, 8);
  root->setSpacing(6);

  m_summary = new QLabel(this);
  m_summary->setWordWrap(true);
  root->addWidget(m_summary);

  m_scrollArea = new QScrollArea(this);
  m_scrollArea->setWidgetResizable(true);
  m_scrollArea->setFrameShape(QFrame::NoFrame);

  m_content = new QWidget;
  m_contentLayout = new QVBoxLayout(m_content);
  m_contentLayout->setContentsMargins(0, 0, 0, 0);
  m_contentLayout->setSpacing(6);
  m_contentLayout->addStretch();

  m_scrollArea->setWidget(m_content);

  root->addWidget(m_scrollArea, 1);

  if (m_model) {
    connect(m_model, &HistoryModel::changed, this, &HistoryPanel::rebuild);
  }

  rebuild();
}

void HistoryPanel::rebuild() {
  if (!m_model || !m_contentLayout) {
    return;
  }

  while (QLayoutItem *item = m_contentLayout->takeAt(0)) {
    if (QWidget *widget = item->widget()) {
      widget->deleteLater();
    }

    delete item;
  }

  const QList<HistoryEntry> &entries = m_model->entries();

  m_summary->setText(tr("%1 recorded edits").arg(entries.size()));

  for (const HistoryEntry &entry : entries) {
    auto *row = new QFrame(m_content);
    row->setFrameShape(QFrame::StyledPanel);

    auto *layout = new QVBoxLayout(row);
    layout->setContentsMargins(8, 6, 8, 6);
    layout->setSpacing(3);

    auto *header = new QLabel(
        tr("%1 — %2")
            .arg(operationLabel(entry.command),
                 QDateTime::fromMSecsSinceEpoch(entry.timestampMs)
                     .toString(QStringLiteral("HH:mm:ss"))),
        row);

    QFont headerFont = header->font();
    headerFont.setBold(true);
    header->setFont(headerFont);

    auto *scopeLabel = new QLabel(
        tr("Scope: %1")
            .arg(entry.command.scopeId.isEmpty() ? tr("(document root)")
                                                  : entry.command.scopeId),
        row);
    scopeLabel->setStyleSheet(QStringLiteral("QLabel { font-family: monospace; }"));

    auto *outcomeLabel = new QLabel(tr("Outcome: %1").arg(entry.outcome), row);

    auto *detailLabel = new QLabel(entry.detail, row);
    detailLabel->setWordWrap(true);

    layout->addWidget(header);
    layout->addWidget(scopeLabel);
    layout->addWidget(outcomeLabel);

    if (!entry.detail.isEmpty()) {
      layout->addWidget(detailLabel);
    }

    m_contentLayout->addWidget(row);
  }

  m_contentLayout->addStretch();
}