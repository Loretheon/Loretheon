#include "../../include/overseer/MemoryPanel.h"

#include "../../include/overseer/MemoryFactCard.h"

#include <QDir>
#include <QFile>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTextStream>
#include <QVBoxLayout>

namespace {

constexpr auto ProseEndMarker = "## Accepted proposals";

} // namespace

MemoryPanel::MemoryPanel(QWidget *parent) : QWidget(parent) {
  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(6, 6, 6, 6);
  root->setSpacing(6);

  m_proseEdit = new QPlainTextEdit(this);
  m_proseEdit->setObjectName(QStringLiteral("memoryProseEdit"));
  m_proseEdit->setPlaceholderText(
      tr("Free-form notes that Overseer sees in every session."));
  m_proseEdit->setFixedHeight(80);
  root->addWidget(m_proseEdit);

  auto *addRow = new QHBoxLayout;
  m_newFactEdit = new QLineEdit(this);
  m_newFactEdit->setPlaceholderText(tr("Add a fact…"));
  m_newFactEdit->setObjectName(QStringLiteral("memoryAddEdit"));

  auto *addButton = new QPushButton(QStringLiteral("+"), this);
  addButton->setObjectName(QStringLiteral("memoryAddButton"));
  addButton->setFixedWidth(28);

  addRow->addWidget(m_newFactEdit, 1);
  addRow->addWidget(addButton);
  root->addLayout(addRow);

  m_scroll = new QScrollArea(this);
  m_scroll->setWidgetResizable(true);
  m_scroll->setFrameShape(QFrame::NoFrame);

  m_host = new QWidget;
  m_hostLayout = new QVBoxLayout(m_host);
  m_hostLayout->setContentsMargins(0, 0, 0, 0);
  m_hostLayout->setSpacing(6);
  m_hostLayout->setAlignment(Qt::AlignTop);
  m_hostLayout->addStretch(1);

  m_scroll->setWidget(m_host);
  root->addWidget(m_scroll, 1);

  connect(addButton, &QPushButton::clicked, this,
          &MemoryPanel::onAddFactClicked);

  connect(m_newFactEdit, &QLineEdit::returnPressed, this,
          &MemoryPanel::onCommitAddFact);

  connect(m_proseEdit, &QPlainTextEdit::textChanged, this,
          &MemoryPanel::onProseEdited);
}

QString MemoryPanel::prose() const {
  return m_proseEdit->toPlainText();
}

void MemoryPanel::loadFromFile(const QString &memoryPath) {
  m_memoryPath = memoryPath;
  m_facts.clear();

  QFile file(memoryPath);
  if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);

    QString prose;
    bool inFacts = false;

    while (!stream.atEnd()) {
      const QString line = stream.readLine();

      if (line.startsWith(ProseEndMarker)) {
        inFacts = true;
        continue;
      }

      if (!inFacts) {
        if (line.startsWith(QStringLiteral("# Memory")))
          continue;
        prose += line + QChar('\n');
        continue;
      }

      const QString trimmed = line.trimmed();
      if (trimmed.startsWith(QStringLiteral("- ")))
        m_facts.append(trimmed.mid(2).trimmed());
    }

    m_proseEdit->blockSignals(true);
    m_proseEdit->setPlainText(prose.trimmed());
    m_proseEdit->blockSignals(false);
  }

  rebuildCards();
}

void MemoryPanel::saveToFile() {
  if (m_memoryPath.isEmpty())
    return;

  QFile file(m_memoryPath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate |
                 QIODevice::Text))
    return;

  QTextStream stream(&file);
  stream.setEncoding(QStringConverter::Utf8);

  stream << "# Memory\n\n";

  const QString p = m_proseEdit->toPlainText().trimmed();
  if (!p.isEmpty())
    stream << p << "\n\n";

  stream << ProseEndMarker << "\n\n";

  for (const QString &fact : std::as_const(m_facts))
    stream << "- " << fact << "\n";

  stream.flush();

  emit changed();
}

void MemoryPanel::rebuildCards() {
  const auto existing = m_host->findChildren<MemoryFactCard *>(
      QString(), Qt::FindDirectChildrenOnly);

  for (auto *card : existing) {
    m_hostLayout->removeWidget(card);
    card->deleteLater();
  }

  for (int i = 0; i < m_facts.size(); ++i) {
    auto *card = new MemoryFactCard(m_facts.at(i), QString(), m_host);

    connect(card, &MemoryFactCard::edited, this,
            [this, card](const QString &newFact) {
              onFactEdited(card, newFact);
            });

    connect(card, &MemoryFactCard::removed, this,
            [this, card]() { onFactRemoved(card); });

    connect(card, &MemoryFactCard::moveToTopRequested, this,
            [this, card]() { onFactMoveTop(card); });

    connect(card, &MemoryFactCard::moveToBottomRequested, this,
            [this, card]() { onFactMoveBottom(card); });

    m_hostLayout->insertWidget(i, card);
  }
}

void MemoryPanel::onAddFactClicked() {
  const QString value = m_newFactEdit->text().trimmed();

  if (value.isEmpty())
    return;

  if (m_facts.contains(value)) {
    m_newFactEdit->clear();
    return;
  }

  m_facts.append(value);
  m_newFactEdit->clear();

  rebuildCards();
  saveToFile();
}

void MemoryPanel::onCommitAddFact() { onAddFactClicked(); }

void MemoryPanel::onFactEdited(MemoryFactCard *card, const QString &newFact) {
  const int idx = m_host->findChildren<MemoryFactCard *>(
      QString(), Qt::FindDirectChildrenOnly).indexOf(card);

  if (idx < 0 || idx >= m_facts.size())
    return;

  if (m_facts.contains(newFact)) {
    card->setFact(m_facts.at(idx));
    return;
  }

  m_facts[idx] = newFact;
  saveToFile();
}

void MemoryPanel::onFactRemoved(MemoryFactCard *card) {
  const int idx = m_host->findChildren<MemoryFactCard *>(
      QString(), Qt::FindDirectChildrenOnly).indexOf(card);

  if (idx < 0 || idx >= m_facts.size())
    return;

  m_facts.removeAt(idx);
  rebuildCards();
  saveToFile();
}

void MemoryPanel::onFactMoveTop(MemoryFactCard *card) {
  const int idx = m_host->findChildren<MemoryFactCard *>(
      QString(), Qt::FindDirectChildrenOnly).indexOf(card);

  if (idx <= 0 || idx >= m_facts.size())
    return;

  const QString value = m_facts.takeAt(idx);
  m_facts.prepend(value);
  rebuildCards();
  saveToFile();
}

void MemoryPanel::onFactMoveBottom(MemoryFactCard *card) {
  const int idx = m_host->findChildren<MemoryFactCard *>(
      QString(), Qt::FindDirectChildrenOnly).indexOf(card);

  if (idx < 0 || idx >= m_facts.size() - 1)
    return;

  const QString value = m_facts.takeAt(idx);
  m_facts.append(value);
  rebuildCards();
  saveToFile();
}

void MemoryPanel::onProseEdited() { saveToFile(); }

void MemoryPanel::ensureSaveButtonState() {}