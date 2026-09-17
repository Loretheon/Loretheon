#include "TranscriptEditPlanCard.h"

#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QVBoxLayout>

namespace {

constexpr int kRowTitleMaxChars = 120;

QString scopeShortLabel(const QString &scope) {
  if (scope.isEmpty() || scope == QStringLiteral("document"))
    return {};
  return scope;
}

QString elideForRow(const QString &text, int maxChars) {
  if (text.size() <= maxChars)
    return text;

  return text.left(maxChars - 3).trimmed() + QStringLiteral("...");
}

} // namespace

TranscriptEditPlanCard::TranscriptEditPlanCard(const QString &planId,
                                               const QString &filePath,
                                               const QString &instruction,
                                               const QJsonArray &commands,
                                               const QString &status,
                                               const QString &result,
                                               QWidget *parent)
    : QWidget(parent),
      m_planId(planId),
      m_filePath(filePath),
      m_instruction(instruction),
      m_commands(commands),
      m_status(status),
      m_result(result) {
  setObjectName(QStringLiteral("transcriptEditPlanCard"));

  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(12, 10, 12, 10);
  root->setSpacing(6);

  const QString fileName =
      filePath.isEmpty()
          ? QObject::tr("(unknown file)")
          : QFileInfo(filePath).fileName();

  m_headerLabel =
      new QLabel(QObject::tr("Editing %1").arg(fileName), this);
  m_headerLabel->setObjectName(QStringLiteral("transcriptPlanHeader"));

  m_instructionLabel = new QLabel(elideForRow(instruction, 240), this);
  m_instructionLabel->setWordWrap(true);
  m_instructionLabel->setToolTip(instruction);
  m_instructionLabel->setObjectName(
      QStringLiteral("transcriptPlanInstruction"));

  root->addWidget(m_headerLabel);
  root->addWidget(m_instructionLabel);

  m_rowsHost = new QWidget(this);
  m_rowsLayout = new QVBoxLayout(m_rowsHost);
  m_rowsLayout->setContentsMargins(0, 4, 0, 4);
  m_rowsLayout->setSpacing(4);

  root->addWidget(m_rowsHost);

  m_resultLabel = new QLabel(result, this);
  m_resultLabel->setWordWrap(true);
  m_resultLabel->setObjectName(QStringLiteral("transcriptPlanResult"));
  m_resultLabel->setVisible(!result.isEmpty());
  root->addWidget(m_resultLabel);

  auto *footer = new QHBoxLayout;
  footer->setContentsMargins(0, 4, 0, 0);
  footer->setSpacing(6);
  footer->addStretch(1);

  m_cancelButton = new QPushButton(tr("Cancel"), this);
  m_applyButton = new QPushButton(tr("Apply selected"), this);

  footer->addWidget(m_cancelButton);
  footer->addWidget(m_applyButton);

  root->addLayout(footer);

  connect(m_applyButton, &QPushButton::clicked, this, [this]() {
    m_applyButton->setEnabled(false);
    m_applyButton->setText(tr("Applying…"));
    m_cancelButton->setEnabled(false);

    emit applyRequested(m_planId);
  });

  connect(m_cancelButton, &QPushButton::clicked, this,
          [this]() { emit cancelRequested(m_planId); });

  rebuildRows();
  updateFooter();
}

bool TranscriptEditPlanCard::isDecidable() const {
  return m_status.isEmpty() || m_status == QStringLiteral("pending");
}

void TranscriptEditPlanCard::setEditAccepted(int editId, bool accepted) {
  for (EditRow &row : m_rows) {
    if (row.id != editId)
      continue;

    row.accepted = accepted;

    if (row.accept)
      row.accept->setEnabled(!accepted);

    if (row.reject)
      row.reject->setEnabled(accepted);

    if (row.widget) {
      row.widget->setProperty("accepted", accepted);
      row.widget->style()->unpolish(row.widget);
      row.widget->style()->polish(row.widget);
      row.widget->update();
    }

    break;
  }
}

void TranscriptEditPlanCard::setPlanStatus(const QString &status,
                                           const QString &result) {
  m_status = status;

  if (!result.isEmpty())
    m_result = result;

  updateRowControls();
  updateFooter();
}

void TranscriptEditPlanCard::updateRowControls() {
  const bool decidable = isDecidable();

  for (EditRow &row : m_rows) {
    if (row.accept)
      row.accept->setVisible(decidable);

    if (row.reject)
      row.reject->setVisible(decidable);

    if (row.statusLabel)
      row.statusLabel->setVisible(!decidable);
  }
}

void TranscriptEditPlanCard::rebuildRows() {
  while (QLayoutItem *item = m_rowsLayout->takeAt(0)) {
    if (QWidget *w = item->widget())
      w->deleteLater();

    delete item;
  }

  m_rows.clear();

  for (int i = 0; i < m_commands.size(); ++i) {
    const QJsonObject command = m_commands.at(i).toObject();

    const int id = i + 1;

    const QString scope = command.value(QStringLiteral("scope")).toString();
    const QString instruction =
        command.value(QStringLiteral("instruction")).toString();

    auto *row = new QWidget(m_rowsHost);
    row->setObjectName(QStringLiteral("transcriptPlanRow"));
    row->setProperty("accepted", true);

    auto *rowLayout = new QHBoxLayout(row);
    rowLayout->setContentsMargins(8, 6, 8, 6);
    rowLayout->setSpacing(10);

    auto *text = new QWidget(row);
    auto *textLayout = new QVBoxLayout(text);
    textLayout->setContentsMargins(0, 0, 0, 0);
    textLayout->setSpacing(2);

    const QString displayTitle = elideForRow(instruction, kRowTitleMaxChars);

    auto *title = new QLabel(displayTitle, text);
    title->setWordWrap(true);
    title->setToolTip(instruction);
    title->setObjectName(QStringLiteral("transcriptPlanRowTitle"));

    textLayout->addWidget(title);

    const QString scopeLabel = scopeShortLabel(scope);

    if (!scopeLabel.isEmpty()) {
      auto *detail = new QLabel(scopeLabel, text);
      detail->setObjectName(QStringLiteral("transcriptPlanRowDetail"));
      textLayout->addWidget(detail);
    }

    auto *accept = new QPushButton(QStringLiteral("\u2713"), row);
    auto *reject = new QPushButton(QStringLiteral("\u2717"), row);

    accept->setFixedWidth(32);
    reject->setFixedWidth(32);
    accept->setToolTip(tr("Include this edit"));
    reject->setToolTip(tr("Exclude this edit"));

    auto *statusLabel = new QLabel(row);
    statusLabel->setObjectName(QStringLiteral("transcriptPlanRowStatus"));
    statusLabel->setVisible(false);

    rowLayout->addWidget(text, 1);
    rowLayout->addWidget(statusLabel);
    rowLayout->addWidget(accept);
    rowLayout->addWidget(reject);

    connect(accept, &QPushButton::clicked, this, [this, id]() {
      setEditAccepted(id, true);
      emit editAccepted(m_planId, id);
    });

    connect(reject, &QPushButton::clicked, this, [this, id]() {
      setEditAccepted(id, false);
      emit editRejected(m_planId, id);
    });

    EditRow editRow;
    editRow.id = id;
    editRow.widget = row;
    editRow.accept = accept;
    editRow.reject = reject;
    editRow.statusLabel = statusLabel;
    editRow.accepted = true;
    editRow.fullTitle = instruction;

    m_rows.append(editRow);

    m_rowsLayout->addWidget(row);
  }

  updateRowControls();
}

void TranscriptEditPlanCard::updateFooter() {
  if (!m_applyButton || !m_cancelButton)
    return;

  const bool pending = isDecidable();

  if (pending) {
    m_applyButton->setEnabled(true);
    m_applyButton->setText(tr("Apply selected"));
    m_cancelButton->setEnabled(true);
  }

  m_applyButton->setVisible(pending);
  m_cancelButton->setVisible(pending);

  m_resultLabel->setText(m_result);
  m_resultLabel->setVisible(!m_result.isEmpty());
}