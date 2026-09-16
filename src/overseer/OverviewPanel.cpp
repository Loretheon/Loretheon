#include "../../include/overseer/OverviewPanel.h"

#include "../../include/overseer/OverseerReferenceCard.h"
#include "../../include/overseer/PathUtils.h"

#include "FileWidget.h"

#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QTextStream>
#include <QVBoxLayout>

OverviewPanel::OverviewPanel(QWidget *parent) : QWidget(parent) {
  setObjectName(QStringLiteral("overviewPanel"));

  auto *root = new QVBoxLayout(this);
  root->setContentsMargins(6, 6, 6, 6);
  root->setSpacing(6);

  auto *addRow = new QHBoxLayout;
  m_addEdit = new QLineEdit(this);
  m_addEdit->setPlaceholderText(tr("Relative path…"));
  m_addEdit->setObjectName(QStringLiteral("overviewAddEdit"));

  auto *addButton = new QPushButton(QStringLiteral("+"), this);
  addButton->setObjectName(QStringLiteral("overviewAddButton"));
  addButton->setFixedWidth(28);

  addRow->addWidget(m_addEdit, 1);
  addRow->addWidget(addButton);
  root->addLayout(addRow);

  m_missingLabel = new QLabel(this);
  m_missingLabel->setObjectName(QStringLiteral("overviewMissingLabel"));
  m_missingLabel->setVisible(false);
  root->addWidget(m_missingLabel);

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

  m_fileWidget = new FileWidget(this);

  auto *splitter = new QSplitter(Qt::Vertical, this);
  splitter->addWidget(m_scroll);
  splitter->addWidget(m_fileWidget);
  splitter->setStretchFactor(0, 1);
  splitter->setStretchFactor(1, 1);

  root->addWidget(splitter, 1);

  connect(m_fileWidget, &FileWidget::fileSelected, this,
          [this](const QString &path) {
            const QString rel = PathUtils::toRelative(path, m_notesRoot);
            if (!rel.isEmpty())
              addRelativePath(rel);
          });

  connect(m_fileWidget, &FileWidget::addToOverseerRequested, this,
          [this](const QStringList &paths) {
            QStringList relatives;
            for (const QString &p : paths) {
              const QString rel = PathUtils::toRelative(p, m_notesRoot);
              if (!rel.isEmpty())
                relatives.append(rel);
            }
            addRelativePaths(relatives);
          });

  setContextMenuPolicy(Qt::CustomContextMenu);

  connect(addButton, &QPushButton::clicked, this,
          &OverviewPanel::onAddPathClicked);

  connect(m_addEdit, &QLineEdit::returnPressed, this,
          &OverviewPanel::onCommitAddPath);

  connect(this, &QWidget::customContextMenuRequested, this,
          [this](const QPoint &pos) {
            QMenu menu(this);
            QAction *addFile = menu.addAction(tr("Add file…"));
            QAction *addFromTree = menu.addAction(tr("Add from tree"));
            QAction *removeMissing =
                menu.addAction(tr("Remove all missing"));

            QAction *chosen = menu.exec(mapToGlobal(pos));

            if (chosen == addFile) {
              onAddPathClicked();
            } else if (chosen == addFromTree) {
              emit addFromTreeRequested();
            } else if (chosen == removeMissing) {
              removeAllMissing();
            }
          });
}

void OverviewPanel::setNotesRoot(const QString &notesRootPath) {
  m_notesRoot = notesRootPath;

  if (m_fileWidget)
    m_fileWidget->setRootPath(m_notesRoot);
}

void OverviewPanel::loadFromFile(const QString &overviewPath) {
  m_overviewPath = overviewPath;
  m_paths.clear();

  QFile file(overviewPath);
  if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    QTextStream stream(&file);
    stream.setEncoding(QStringConverter::Utf8);

    while (!stream.atEnd()) {
      const QString line = stream.readLine().trimmed();

      if (!line.startsWith(QStringLiteral("- ")))
        continue;

      QString path = line.mid(2).trimmed();

      if (path.isEmpty())
        continue;

      if (path.startsWith(QChar('/'))) {
        const QString rel = PathUtils::toRelative(path, m_notesRoot);
        path = rel.isEmpty() ? path : rel;
      }

      if (!m_paths.contains(path))
        m_paths.append(path);
    }
  }

  rebuildCards();
}

void OverviewPanel::saveToFile() {
  if (m_overviewPath.isEmpty())
    return;

  QFile file(m_overviewPath);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate |
                 QIODevice::Text))
    return;

  QTextStream stream(&file);
  stream.setEncoding(QStringConverter::Utf8);

  for (const QString &path : std::as_const(m_paths))
    stream << "- " << path << "\n";

  stream.flush();

  emit changed();
}

void OverviewPanel::addRelativePath(const QString &relativePath) {
  addRelativePaths({relativePath});
}

void OverviewPanel::addRelativePaths(const QStringList &relativePaths) {
  bool anyAdded = false;

  for (const QString &path : relativePaths) {
    if (path.isEmpty())
      continue;

    if (m_paths.contains(path))
      continue;

    m_paths.append(path);
    anyAdded = true;
  }

  if (anyAdded) {
    rebuildCards();
    saveToFile();
  }
}

void OverviewPanel::removeAllMissing() {
  bool changed = false;

  for (int i = m_paths.size() - 1; i >= 0; --i) {
    const QString abs = PathUtils::toAbsolute(m_paths.at(i), m_notesRoot);

    if (!QFileInfo::exists(abs)) {
      m_paths.removeAt(i);
      changed = true;
    }
  }

  if (changed) {
    rebuildCards();
    saveToFile();
  }
}

void OverviewPanel::rebuildCards() {
  const auto existing = m_host->findChildren<OverseerReferenceCard *>(
      QString(), Qt::FindDirectChildrenOnly);

  for (auto *card : existing) {
    m_hostLayout->removeWidget(card);
    card->deleteLater();
  }

  int missingCount = 0;

  for (int i = 0; i < m_paths.size(); ++i) {
    auto *card =
        new OverseerReferenceCard(m_paths.at(i), m_notesRoot, m_host);

    if (!card->exists())
      ++missingCount;

    connect(card, &OverseerReferenceCard::removed, this,
            [this, card]() { onCardRemoved(card); });

    connect(card, &OverseerReferenceCard::openRequested, this,
            &OverviewPanel::openRequested);

    connect(card, &OverseerReferenceCard::openInNormalEditorRequested, this,
            &OverviewPanel::openInNormalEditorRequested);

    connect(card, &OverseerReferenceCard::stageRequested, this,
            &OverviewPanel::stageRequested);

    m_hostLayout->insertWidget(i, card);
  }

  updateMissingHeader();
}

void OverviewPanel::updateMissingHeader() {
  int missing = 0;

  for (const QString &p : std::as_const(m_paths)) {
    const QString abs = PathUtils::toAbsolute(p, m_notesRoot);
    if (!QFileInfo::exists(abs))
      ++missing;
  }

  if (missing == 0) {
    m_missingLabel->setVisible(false);
    return;
  }

  m_missingLabel->setText(
      tr("%n missing reference(s).", "", missing));
  m_missingLabel->setVisible(true);
}

void OverviewPanel::onAddPathClicked() {
  const QString value = m_addEdit->text().trimmed();

  if (value.isEmpty())
    return;

  QString path = value;

  if (path.startsWith(QChar('/'))) {
    const QString rel = PathUtils::toRelative(path, m_notesRoot);
    path = rel.isEmpty() ? path : rel;
  }

  m_addEdit->clear();

  addRelativePath(path);
}

void OverviewPanel::onCommitAddPath() { onAddPathClicked(); }

void OverviewPanel::onCardRemoved(OverseerReferenceCard *card) {
  const auto cards = m_host->findChildren<OverseerReferenceCard *>(
      QString(), Qt::FindDirectChildrenOnly);

  const int idx = cards.indexOf(card);

  if (idx < 0 || idx >= m_paths.size())
    return;

  m_paths.removeAt(idx);
  rebuildCards();
  saveToFile();
}