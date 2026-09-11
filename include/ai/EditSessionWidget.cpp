#include "EditSessionWidget.h"

#include <QButtonGroup>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QRadioButton>
#include <QScrollArea>
#include <QSizePolicy>
#include <QTextEdit>
#include <QVBoxLayout>

namespace {

QLabel *createSectionLabel(const QString &text, QWidget *parent) {
  auto *label = new QLabel(text, parent);

  QFont font = label->font();

  font.setBold(true);

  label->setFont(font);

  label->setStyleSheet(QStringLiteral("QLabel {"
                                      "    margin-top: 6px;"
                                      "    margin-bottom: 2px;"
                                      "}"));

  return label;
}

QTextEdit *createReadOnlyText(QWidget *parent) {
  auto *edit = new QTextEdit(parent);

  edit->setReadOnly(true);
  edit->setAcceptRichText(false);

  edit->setLineWrapMode(QTextEdit::WidgetWidth);

  edit->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);

  edit->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  edit->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

  edit->setStyleSheet(QStringLiteral("QTextEdit {"
                                     "    border: 1px solid palette(mid);"
                                     "    border-radius: 4px;"
                                     "    background: palette(base);"
                                     "    padding: 6px;"
                                     "}"));

  return edit;
}

QWidget *createSection(const QString &title, QTextEdit *editor,
                       QWidget *parent) {
  auto *container = new QWidget(parent);

  auto *layout = new QVBoxLayout(container);

  layout->setContentsMargins(0, 0, 0, 0);

  layout->setSpacing(3);

  layout->addWidget(createSectionLabel(title, container));

  layout->addWidget(editor);

  return container;
}

} // namespace

EditSessionWidget::EditSessionWidget(QWidget *parent) : QWidget(parent) {
  auto *rootLayout = new QVBoxLayout(this);

  rootLayout->setContentsMargins(0, 0, 0, 0);

  rootLayout->setSpacing(6);

  m_scrollArea = new QScrollArea(this);

  m_scrollArea->setWidgetResizable(true);
  m_scrollArea->setFrameShape(QFrame::NoFrame);
  m_scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

  m_historyContainer = new QWidget;

  m_historyLayout = new QVBoxLayout(m_historyContainer);

  m_historyLayout->setContentsMargins(8, 8, 8, 8);

  m_historyLayout->setSpacing(10);
  m_historyLayout->addStretch();

  m_scrollArea->setWidget(m_historyContainer);

  rootLayout->addWidget(m_scrollArea);

  m_batchBar = new QWidget(this);

  auto *batchLayout = new QHBoxLayout(m_batchBar);

  batchLayout->setContentsMargins(8, 0, 8, 0);

  batchLayout->setSpacing(6);

  m_acceptAllButton = new QPushButton(tr("Accept All"), m_batchBar);

  m_rejectAllButton = new QPushButton(tr("Reject All"), m_batchBar);

  m_applyButton = new QPushButton(tr("Apply Selected"), m_batchBar);

  m_skipButton = new QPushButton(tr("Skip"), m_batchBar);

  batchLayout->addWidget(m_acceptAllButton);

  batchLayout->addWidget(m_rejectAllButton);

  batchLayout->addWidget(m_applyButton);

  batchLayout->addStretch();

  batchLayout->addWidget(m_skipButton);

  rootLayout->addWidget(m_batchBar);

  connect(m_acceptAllButton, &QPushButton::clicked, this,
          &EditSessionWidget::acceptAllPendingEditsRequested);

  connect(m_rejectAllButton, &QPushButton::clicked, this,
          &EditSessionWidget::rejectAllPendingEditsRequested);

  connect(m_applyButton, &QPushButton::clicked, this,
          &EditSessionWidget::applyAcceptedPendingEditsRequested);

  connect(m_skipButton, &QPushButton::clicked, this,
          &EditSessionWidget::skipRequested);

  m_batchBar->hide();
}

void EditSessionWidget::clearHistory() {
  for (EditCard *card : std::as_const(m_cards)) {

    if (!card) {
      continue;
    }

    delete card->widget;
    delete card;
  }

  m_cards.clear();

  for (auto &ui : m_conflictGroups) {

    delete ui.wrapper;
    ui.wrapper = nullptr;
  }

  m_conflictGroups.clear();

  m_batchBar->hide();
}

void EditSessionWidget::startEdit(int editNumber, const QString &request) {
  EditCard *card = cardFor(editNumber);

  if (!card) {
    card = createEditCard(editNumber, request);
  } else {
    card->requestEdit->setPlainText(request);
  }

  card->reviewVisible = false;

  updateReviewControls(card);

  setStatus(editNumber, QStringLiteral("Resolving"));
}

void EditSessionWidget::setCommand(int editNumber, const QString &command) {
  EditCard *card = cardFor(editNumber);

  if (!card) {
    card = createEditCard(editNumber, QString());
  }

  card->commandEdit->setPlainText(command);
}

void EditSessionWidget::setTarget(int editNumber, const QString &target) {
  EditCard *card = cardFor(editNumber);

  if (!card) {
    card = createEditCard(editNumber, QString());
  }

  card->targetEdit->setPlainText(target);

  setStatus(editNumber, QStringLiteral("Writing"));
}

void EditSessionWidget::setStatus(int editNumber, const QString &status) {
  EditCard *card = cardFor(editNumber);

  if (!card) {
    card = createEditCard(editNumber, QString());
  }

  card->statusLabel->setText(status);

  updateStatusAppearance(card, status);

  const QString normalized = status.trimmed().toLower();

  if (normalized == QStringLiteral("ready for review")) {

    card->reviewVisible = true;

    updateReviewControls(card);

    m_batchBar->show();
  }
}

void EditSessionWidget::setPendingEditReady(int editNumber) {
  EditCard *card = cardFor(editNumber);

  if (!card) {
    return;
  }

  card->reviewVisible = true;

  updateReviewControls(card);

  setStatus(editNumber, QStringLiteral("Ready for review"));

  m_batchBar->show();
}

void EditSessionWidget::setPendingEditDecision(int editNumber, bool accepted) {
  EditCard *card = cardFor(editNumber);

  if (!card) {
    return;
  }

  card->accepted = accepted;

  card->reviewVisible = true;

  updateReviewControls(card);

  card->statusLabel->setText(accepted ? tr("Accepted") : tr("Rejected"));

  updateStatusAppearance(card, card->statusLabel->text());

  m_batchBar->show();
}

void EditSessionWidget::finishEdit(int editNumber, const QString &status) {
  EditCard *card = cardFor(editNumber);

  if (!card) {
    card = createEditCard(editNumber, QString());
  }

  setResultText(card, status);

  setStatus(editNumber, status);
}

void EditSessionWidget::failEdit(int editNumber, const QString &reason) {
  EditCard *card = cardFor(editNumber);

  if (!card) {
    card = createEditCard(editNumber, QString());
  }

  card->reviewVisible = false;

  setResultText(card, reason);

  setStatus(editNumber, QStringLiteral("Failed"));
}

void EditSessionWidget::abortEdit(int editNumber, const QString &reason) {
  EditCard *card = cardFor(editNumber);

  if (!card) {
    card = createEditCard(editNumber, QString());
  }

  card->reviewVisible = false;

  setResultText(card, reason);

  setStatus(editNumber, QStringLiteral("Aborted"));
}

EditSessionWidget::EditCard *
EditSessionWidget::createEditCard(int editNumber, const QString &request) {
  auto *card = new EditCard;

  card->widget = new QFrame(m_historyContainer);

  card->widget->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);

  auto *layout = new QVBoxLayout(card->widget);

  layout->setContentsMargins(12, 10, 12, 12);

  layout->setSpacing(5);

  auto *header = new QHBoxLayout;

  card->statusIndicator = new QWidget(card->widget);

  card->statusIndicator->setFixedSize(9, 9);

  card->titleLabel = new QLabel(tr("Edit %1").arg(editNumber), card->widget);

  QFont titleFont = card->titleLabel->font();

  titleFont.setBold(true);
  titleFont.setPointSize(titleFont.pointSize() + 1);

  card->titleLabel->setFont(titleFont);

  card->statusLabel = new QLabel(tr("Waiting"), card->widget);

  card->statusLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

  QFont statusFont = card->statusLabel->font();

  statusFont.setBold(true);

  card->statusLabel->setFont(statusFont);

  header->addWidget(card->statusIndicator);

  header->addSpacing(7);

  header->addWidget(card->titleLabel);

  header->addStretch();

  header->addWidget(card->statusLabel);

  layout->addLayout(header);

  card->requestEdit = createReadOnlyText(card->widget);

  card->requestEdit->setPlainText(request);

  card->requestEdit->setMinimumHeight(55);

  card->requestEdit->setMaximumHeight(140);

  layout->addWidget(
      createSection(tr("Request"), card->requestEdit, card->widget));

  card->commandEdit = createReadOnlyText(card->widget);

  card->commandEdit->setMinimumHeight(75);

  card->commandEdit->setMaximumHeight(180);

  QFont codeFont = card->commandEdit->font();

  codeFont.setFamily(QStringLiteral("monospace"));

  card->commandEdit->setFont(codeFont);

  layout->addWidget(
      createSection(tr("Command"), card->commandEdit, card->widget));

  card->targetEdit = createReadOnlyText(card->widget);

  card->targetEdit->setMinimumHeight(55);

  card->targetEdit->setMaximumHeight(200);

  layout->addWidget(
      createSection(tr("Target"), card->targetEdit, card->widget));

  card->resultEdit = createReadOnlyText(card->widget);

  card->resultEdit->setMinimumHeight(50);

  card->resultEdit->setMaximumHeight(180);

  layout->addWidget(
      createSection(tr("Result"), card->resultEdit, card->widget));

  createReviewControls(card, editNumber);

  layout->addWidget(card->reviewBar);

  m_historyLayout->insertWidget(m_historyLayout->count() - 1, card->widget);

  m_cards.insert(editNumber, card);

  updateReviewControls(card);

  return card;
}

void EditSessionWidget::createReviewControls(EditCard *card, int editNumber) {
  if (!card || card->reviewBar) {
    return;
  }

  card->reviewBar = new QWidget(card->widget);

  auto *layout = new QHBoxLayout(card->reviewBar);

  layout->setContentsMargins(0, 6, 0, 0);

  layout->setSpacing(6);

  auto *label = new QLabel(tr("Review this edit"), card->reviewBar);

  layout->addWidget(label);
  layout->addStretch();

  card->acceptButton = new QPushButton(tr("Accept"), card->reviewBar);

  card->rejectButton = new QPushButton(tr("Reject"), card->reviewBar);

  layout->addWidget(card->acceptButton);

  layout->addWidget(card->rejectButton);

  connect(card->acceptButton, &QPushButton::clicked, this,
          [this, editNumber]() { emit pendingEditAccepted(editNumber); });

  connect(card->rejectButton, &QPushButton::clicked, this,
          [this, editNumber]() { emit pendingEditRejected(editNumber); });
}

void EditSessionWidget::updateReviewControls(EditCard *card) {
  if (!card) {
    return;
  }

  card->reviewBar->setVisible(card->reviewVisible);

  card->acceptButton->setEnabled(!card->accepted);

  card->rejectButton->setEnabled(card->accepted);

  if (card->accepted) {
    card->acceptButton->setText(tr("Accepted"));

    card->rejectButton->setText(tr("Reject"));
  } else {
    card->acceptButton->setText(tr("Accept"));

    card->rejectButton->setText(tr("Rejected"));
  }
}

EditSessionWidget::EditCard *EditSessionWidget::cardFor(int editNumber) const {
  return m_cards.value(editNumber, nullptr);
}

void EditSessionWidget::setResultText(EditCard *card, const QString &text) {
  if (!card) {
    return;
  }

  card->resultEdit->setPlainText(text);
}

void EditSessionWidget::updateStatusAppearance(EditCard *card,
                                               const QString &status) {
  if (!card) {
    return;
  }

  const QString normalized = status.trimmed().toLower();

  QString border = QStringLiteral("palette(mid)");

  QString indicator = QStringLiteral("palette(mid)");

  if (normalized == QStringLiteral("accepted")) {

    border = QStringLiteral("#4caf50");

    indicator = QStringLiteral("#4caf50");

  } else if (normalized == QStringLiteral("rejected") ||
             normalized == QStringLiteral("failed")) {

    border = QStringLiteral("#d32f2f");

    indicator = QStringLiteral("#d32f2f");

  } else if (normalized == QStringLiteral("aborted")) {

    border = QStringLiteral("#f57c00");

    indicator = QStringLiteral("#f57c00");

  } else if (normalized == QStringLiteral("conflicting")) {

    border = QStringLiteral("#ab47bc");

    indicator = QStringLiteral("#ab47bc");

  } else if (normalized == QStringLiteral("writing") ||
             normalized == QStringLiteral("resolving") ||
             normalized == QStringLiteral("ready for review")) {

    border = QStringLiteral("#1976d2");

    indicator = QStringLiteral("#1976d2");
  }

  card->widget->setStyleSheet(QStringLiteral("QFrame {"
                                             "    border: 1px solid %1;"
                                             "    border-radius: 7px;"
                                             "    background: palette(window);"
                                             "}")
                                  .arg(border));

  card->statusIndicator->setStyleSheet(QStringLiteral("QWidget {"
                                                      "    background: %1;"
                                                      "    border-radius: 4px;"
                                                      "    border: none;"
                                                      "}")
                                           .arg(indicator));
}

void EditSessionWidget::showConflictGroup(int groupId,
                                          const QVector<int> &editNumbers) {
  if (editNumbers.size() < 2) {
    return;
  }

  for (int editNumber : editNumbers) {

    if (!cardFor(editNumber)) {
      createEditCard(editNumber, QString());
    }

    setStatus(editNumber, QStringLiteral("Conflicting"));
  }

  ConflictGroupUi ui;

  ui.groupId = groupId;

  ui.editNumbers = editNumbers;

  m_conflictGroups.insert(groupId, ui);

  rebuildConflictWrapper(m_conflictGroups[groupId]);
}

void EditSessionWidget::clearConflictGroup(int groupId) {
  if (!m_conflictGroups.contains(groupId)) {
    return;
  }

  auto &ui = m_conflictGroups[groupId];

  delete ui.wrapper;

  ui.wrapper = nullptr;

  m_conflictGroups.remove(groupId);
}

void EditSessionWidget::rebuildConflictWrapper(ConflictGroupUi &ui) {
  if (ui.wrapper) {
    delete ui.wrapper;
    ui.wrapper = nullptr;
  }

  ui.wrapper = new QFrame(m_historyContainer);

  auto *layout = new QVBoxLayout(ui.wrapper);

  layout->setContentsMargins(10, 10, 10, 10);

  layout->setSpacing(6);

  auto *header =
      new QLabel(tr("These edits overlap and cannot both apply."), ui.wrapper);

  header->setWordWrap(true);

  layout->addWidget(header);

  ui.choiceGroup = new QButtonGroup(ui.wrapper);

  for (int editNumber : ui.editNumbers) {

    auto *radio =
        new QRadioButton(tr("Keep edit %1").arg(editNumber), ui.wrapper);

    radio->setProperty("editNumber", editNumber);

    ui.choiceGroup->addButton(radio);

    layout->addWidget(radio);
  }

  auto *actions = new QHBoxLayout;

  actions->addStretch();

  ui.discardButton = new QPushButton(tr("Discard both"), ui.wrapper);

  ui.abortButton = new QPushButton(tr("Abort batch"), ui.wrapper);

  actions->addWidget(ui.discardButton);

  actions->addWidget(ui.abortButton);

  layout->addLayout(actions);

  const int groupId = ui.groupId;

  connect(ui.choiceGroup, &QButtonGroup::buttonClicked, this,
          [this, groupId](QAbstractButton *button) {
            emit conflictResolved(groupId,
                                  button->property("editNumber").toInt());
          });

  connect(ui.discardButton, &QPushButton::clicked, this,
          [this, groupId]() { emit conflictGroupDiscarded(groupId); });

  connect(ui.abortButton, &QPushButton::clicked, this,
          &EditSessionWidget::conflictBatchAborted);

  m_historyLayout->insertWidget(m_historyLayout->count() - 1, ui.wrapper);
}