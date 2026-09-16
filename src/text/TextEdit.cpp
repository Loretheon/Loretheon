#include "TextEdit.h"
#include <QContextMenuEvent>
#include <QMenu>
#include "../../include/text/Toolbar.h"
#include "../../include/text/formats/HTMLFormatDelegate.h"
#include "../../include/text/formats/MarkdownFormatDelegate.h"
#include "DocumentMode.h"
#include "TextDocument.h"

#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSettings>
#include <QStringList>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QTextStream>
#include <QToolTip>
#include <QVBoxLayout>

#include <algorithm>

namespace {

constexpr auto LastOpenedFileKey = "document/lastOpenedFile";
constexpr auto AutoAcceptEditsKey = "editing/autoAcceptEdits";
constexpr auto HighlightModeKey = "editing/highlightMode";

constexpr int ReviewBarHorizontalMargin = 8;
constexpr int ReviewBarVerticalMargin = 6;
constexpr int ReviewRowSpacing = 6;
constexpr int ReviewBarMaxHeightDivisor = 3;

QString documentModeSuffix(const QString &suffix) {
  const QString normalized = suffix.toLower();

  if (normalized == QStringLiteral("md") ||
      normalized == QStringLiteral("markdown") ||
      normalized == QStringLiteral("mdown") ||
      normalized == QStringLiteral("mkd")) {
    return QStringLiteral("markdown");
  }

  if (normalized == QStringLiteral("html") ||
      normalized == QStringLiteral("htm") ||
      normalized == QStringLiteral("xhtml")) {
    return QStringLiteral("html");
  }

  return {};
}

} // namespace

TextEdit::TextEdit(QWidget *parent) : QPlainTextEdit(parent) {
  QSettings settings;

  setMouseTracking(true);

  m_autoAcceptEdits = settings.value(AutoAcceptEditsKey, false).toBool();

  const int storedHighlightMode =
      settings.value(HighlightModeKey, 0).toInt();

  switch (storedHighlightMode) {
  case 1:
    m_highlightMode = HighlightMode::Sent;
    break;
  case 2:
    m_highlightMode = HighlightMode::Both;
    break;
  default:
    m_highlightMode = HighlightMode::Intent;
    break;
  }

  setupToolbar();
  setupReviewBar();
  setDocumentMode(DocumentMode::Markdown);

  connect(this, &QPlainTextEdit::cursorPositionChanged, this,
          &TextEdit::formatChanged);

  updateReviewBar();
}

const PendingEdit *TextEdit::pendingEditAtPosition(int position) const {
  for (const PendingEdit &edit : std::as_const(m_pendingEdits)) {
    if (!edit.match.isValid()) {
      continue;
    }

    if (position >= edit.match.start && position <= edit.match.end) {
      return &edit;
    }
  }

  return nullptr;
}

QString TextEdit::pendingEditPreview(const PendingEdit &edit) const {
  // Use the generated content directly. Do not rely on match.matchedText
  // because it is empty for insertions and for whole-scope replacement.
  switch (edit.command.operation) {
  case EditCommand::Operation::Insert:
  case EditCommand::Operation::Replace:
  case EditCommand::Operation::ReplaceScope:
    return edit.generatedText;

  case EditCommand::Operation::Delete:
    return edit.command.findString.isEmpty() ? QString()
                                              : edit.command.findString;
  }

  return {};
}

void TextEdit::mouseMoveEvent(QMouseEvent *event) {
  QPlainTextEdit::mouseMoveEvent(event);

  if (!event) {
    QToolTip::hideText();
    return;
  }

  const QTextCursor cursor = cursorForPosition(event->position().toPoint());

  const PendingEdit *edit = pendingEditAtPosition(cursor.position());

  if (!edit) {
    QToolTip::hideText();
    return;
  }

  const QString preview = pendingEditPreview(*edit);

  if (preview.isEmpty()) {
    QToolTip::hideText();
    return;
  }

  QToolTip::showText(event->globalPosition().toPoint(), preview, this);
}

void TextEdit::setupToolbar() {
  m_toolbar = new Toolbar(this);

  m_toolbar->setGeometry(0, 0, width(), m_toolbar->sizeHint().height());

  setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
  setViewportMargins(0, m_toolbar->sizeHint().height(), 0, 0);

  setupToolbarConnections();
}

void TextEdit::setupToolbarConnections() {
  if (!m_toolbar) {
    return;
  }

  m_toolbar->setTextEdit(this);
}

void TextEdit::setToolbarVisible(bool visible) {
  if (m_toolbarVisible == visible)
    return;

  m_toolbarVisible = visible;

  if (m_toolbar) {
    m_toolbar->setVisible(visible);

    const int toolbarHeight = visible ? m_toolbar->sizeHint().height() : 0;

    setViewportMargins(0, toolbarHeight, 0,
                       m_reviewBar && m_reviewBar->isVisible()
                           ? qMin(m_reviewBar->sizeHint().height(),
                                  height() / ReviewBarMaxHeightDivisor)
                           : 0);
  }
}

void TextEdit::setupReviewBar() {
  m_reviewBar = new QWidget(this);

  m_reviewSummary = new QLabel(m_reviewBar);
  m_reviewSummary->setTextInteractionFlags(Qt::NoTextInteraction);

  m_acceptAllButton = new QPushButton(tr("Accept All"), m_reviewBar);

  m_rejectAllButton = new QPushButton(tr("Reject All"), m_reviewBar);

  m_autoAcceptCheckBox = new QCheckBox(tr("Auto-accept"), m_reviewBar);

  m_autoAcceptCheckBox->setChecked(m_autoAcceptEdits);

  m_highlightModeCombo = new QComboBox(m_reviewBar);

  m_highlightModeCombo->addItem(tr("Intent"), QVariant::fromValue(0));
  m_highlightModeCombo->addItem(tr("Sent"), QVariant::fromValue(1));
  m_highlightModeCombo->addItem(tr("Both"), QVariant::fromValue(2));

  m_highlightModeCombo->setToolTip(
      tr("Choose how scopes referenced by the assistant are highlighted."));

  switch (m_highlightMode) {
  case HighlightMode::Sent:
    m_highlightModeCombo->setCurrentIndex(1);
    break;
  case HighlightMode::Both:
    m_highlightModeCombo->setCurrentIndex(2);
    break;
  case HighlightMode::Intent:
  default:
    m_highlightModeCombo->setCurrentIndex(0);
    break;
  }

  m_reviewScrollArea = new QScrollArea(m_reviewBar);

  m_reviewScrollArea->setWidgetResizable(true);
  m_reviewScrollArea->setFrameShape(QFrame::NoFrame);

  m_reviewContent = new QWidget;

  m_reviewLayout = new QVBoxLayout(m_reviewContent);

  m_reviewLayout->setContentsMargins(0, 0, 0, 0);
  m_reviewLayout->setSpacing(4);

  m_reviewScrollArea->setWidget(m_reviewContent);

  auto *controls = new QHBoxLayout;

  controls->setContentsMargins(0, 0, 0, 0);

  controls->addWidget(m_reviewSummary);
  controls->addStretch();
  controls->addWidget(m_highlightModeCombo);
  controls->addWidget(m_acceptAllButton);
  controls->addWidget(m_rejectAllButton);
  controls->addWidget(m_autoAcceptCheckBox);

  auto *layout = new QVBoxLayout(m_reviewBar);

  layout->setContentsMargins(ReviewBarHorizontalMargin, ReviewBarVerticalMargin,
                             ReviewBarHorizontalMargin,
                             ReviewBarVerticalMargin);

  layout->setSpacing(ReviewRowSpacing);
  layout->addLayout(controls);
  layout->addWidget(m_reviewScrollArea);

  connect(m_acceptAllButton, &QPushButton::clicked, this,
          &TextEdit::acceptAllPendingEditsRequested);

  connect(m_rejectAllButton, &QPushButton::clicked, this,
          &TextEdit::rejectAllPendingEditsRequested);

  connect(m_autoAcceptCheckBox, &QCheckBox::toggled, this,
          &TextEdit::setAutoAcceptEdits);

  connect(m_highlightModeCombo, &QComboBox::currentIndexChanged, this,
          [this](int index) {
            switch (index) {
            case 1:
              setHighlightMode(HighlightMode::Sent);
              break;
            case 2:
              setHighlightMode(HighlightMode::Both);
              break;
            case 0:
            default:
              setHighlightMode(HighlightMode::Intent);
              break;
            }
          });

  m_reviewBar->hide();
}

void TextEdit::setDocumentMode(DocumentMode mode) {
  if (m_mode == mode && m_delegate) {
    return;
  }

  m_mode = mode;

  switch (mode) {
  case DocumentMode::Markdown:
    m_delegate = std::make_unique<MarkdownFormatDelegate>();
    break;

  case DocumentMode::Html:
    m_delegate = std::make_unique<HTMLFormatDelegate>();
    break;

  case DocumentMode::PlainText:
    m_delegate.reset();
    break;

  case DocumentMode::Dot:
  case DocumentMode::PlantUml:
  case DocumentMode::Mermaid:
    m_delegate.reset();
    break;
  }

  emit formatChanged();
}

bool TextEdit::openFile(const QString &filePath) {
  QFile file(filePath);

  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return false;
  }

  QTextStream stream(&file);
  stream.setEncoding(QStringConverter::Utf8);

  const QString text = stream.readAll();

  if (stream.status() != QTextStream::Ok) {
    return false;
  }

  setPlainText(text);

  const QString mode = documentModeSuffix(QFileInfo(filePath).suffix());

  if (mode == QStringLiteral("markdown")) {
    setDocumentMode(DocumentMode::Markdown);
  } else if (mode == QStringLiteral("html")) {
    setDocumentMode(DocumentMode::Html);
  } else {
    setDocumentMode(DocumentMode::PlainText);
  }

  QSettings settings;

  settings.setValue(LastOpenedFileKey, QFileInfo(filePath).absoluteFilePath());

  clearPendingEdits();

  return true;
}

QString TextEdit::lastOpenedFile() {
  QSettings settings;
  return settings.value(LastOpenedFileKey).toString();
}

void TextEdit::setAutoAcceptEdits(bool enabled) {
  if (m_autoAcceptEdits == enabled) {
    return;
  }

  m_autoAcceptEdits = enabled;

  QSettings settings;
  settings.setValue(AutoAcceptEditsKey, enabled);

  emit autoAcceptChanged(enabled);
  updateReviewBar();
}

void TextEdit::setPendingEditAccepted(int editId, bool accepted) {
  auto it = m_pendingEdits.find(editId);

  if (it == m_pendingEdits.end()) {
    return;
  }

  if (it->accepted == accepted) {
    return;
  }

  it->accepted = accepted;

  updateReviewBar();
  updateAllHighlights();

  if (accepted) {
    emit acceptPendingEditRequested(editId);
  } else {
    emit rejectPendingEditRequested(editId);
  }
}

void TextEdit::showPendingEdit(const PendingEdit &edit) {
  m_pendingEdits.insert(edit.id, edit);

  updateAllHighlights();
  updateReviewBar();
}

void TextEdit::updatePendingEdit(const PendingEdit &edit) {
  m_pendingEdits.insert(edit.id, edit);

  updateAllHighlights();
  updateReviewBar();
}

void TextEdit::removePendingEdit(int editId) {
  m_pendingEdits.remove(editId);

  updateAllHighlights();
  updateReviewBar();
}

void TextEdit::clearPendingEdits() {
  m_pendingEdits.clear();

  updateAllHighlights();
  updateReviewBar();
}

void TextEdit::refreshPendingEdits() {
  updateAllHighlights();
  updateReviewBar();
}

void TextEdit::setHighlightedScopes(const QStringList &scopeIds) {
  if (m_highlightedScopes == scopeIds) {
    return;
  }

  m_highlightedScopes = scopeIds;

  updateAllHighlights();
}

void TextEdit::clearHighlightedScopes() {
  if (m_highlightedScopes.isEmpty()) {
    return;
  }

  m_highlightedScopes.clear();

  updateAllHighlights();
}

void TextEdit::setHighlightMode(HighlightMode mode) {
  if (m_highlightMode == mode) {
    return;
  }

  m_highlightMode = mode;

  QSettings settings;
  settings.setValue(HighlightModeKey, static_cast<int>(mode));

  if (m_highlightModeCombo) {
    int index = 0;

    switch (mode) {
    case HighlightMode::Sent:
      index = 1;
      break;
    case HighlightMode::Both:
      index = 2;
      break;
    case HighlightMode::Intent:
    default:
      index = 0;
      break;
    }

    if (m_highlightModeCombo->currentIndex() != index) {
      QSignalBlocker blocker(m_highlightModeCombo);
      m_highlightModeCombo->setCurrentIndex(index);
    }
  }

  updateAllHighlights();
}

void TextEdit::rebuildReviewRows() {
  if (!m_reviewLayout) {
    return;
  }

  while (QLayoutItem *item = m_reviewLayout->takeAt(0)) {
    if (QWidget *widget = item->widget()) {
      widget->deleteLater();
    }

    delete item;
  }

  QList<int> ids = m_pendingEdits.keys();

  std::sort(ids.begin(), ids.end());

  for (const int id : ids) {
    m_reviewLayout->addWidget(createReviewRow(m_pendingEdits.value(id)));
  }
}

QWidget *TextEdit::createReviewRow(const PendingEdit &edit) {
  auto *row = new QWidget(m_reviewContent);

  setReviewRowStyle(row, edit);

  auto *title = new QLabel(row);
  title->setText(tr("Edit %1 — %2").arg(edit.id).arg(pendingEditSummary(edit)));

  auto *preview = new QLabel(row);
  preview->setWordWrap(true);

  switch (edit.command.operation) {
  case EditCommand::Operation::Delete:
    preview->setText(edit.command.findString.isEmpty()
                         ? tr("Delete content")
                         : edit.command.findString);
    break;

  case EditCommand::Operation::Insert:
  case EditCommand::Operation::Replace:
  case EditCommand::Operation::ReplaceScope:
    preview->setText(edit.generatedText.isEmpty() ? tr("Generating…")
                                                  : edit.generatedText);
    break;
  }

  auto *acceptButton = new QPushButton(tr("Accept"), row);

  auto *rejectButton = new QPushButton(tr("Reject"), row);

  connect(acceptButton, &QPushButton::clicked, this, [this, editId = edit.id] {
    emit acceptPendingEditRequested(editId);
  });

  connect(rejectButton, &QPushButton::clicked, this, [this, editId = edit.id] {
    emit rejectPendingEditRequested(editId);
  });

  acceptButton->setEnabled(!edit.accepted);
  rejectButton->setEnabled(edit.accepted);

  auto *textLayout = new QVBoxLayout;

  textLayout->setContentsMargins(0, 0, 0, 0);
  textLayout->addWidget(title);
  textLayout->addWidget(preview);

  auto *layout = new QHBoxLayout(row);

  layout->setContentsMargins(6, 5, 6, 5);
  layout->setSpacing(ReviewRowSpacing);
  layout->addLayout(textLayout, 1);
  layout->addWidget(acceptButton);
  layout->addWidget(rejectButton);

  return row;
}

void TextEdit::setReviewRowStyle(QWidget *row, const PendingEdit &edit) {
  Q_UNUSED(row);
  Q_UNUSED(edit);
}

void TextEdit::updateReviewBar() {
  if (!m_reviewBar) {
    return;
  }

  if (m_pendingEdits.isEmpty()) {
    m_reviewBar->hide();

    QResizeEvent event(size(), size());

    resizeEvent(&event);
    return;
  }

  rebuildReviewRows();

  const int acceptedCount =
      std::count_if(m_pendingEdits.cbegin(), m_pendingEdits.cend(),
                    [](const PendingEdit &edit) { return edit.accepted; });

  m_reviewSummary->setText(tr("%1 edits pending · %2 selected")
                               .arg(m_pendingEdits.size())
                               .arg(acceptedCount));

  m_autoAcceptCheckBox->setChecked(m_autoAcceptEdits);

  m_reviewBar->show();

  QResizeEvent event(size(), size());

  resizeEvent(&event);
}

void TextEdit::updatePendingHighlight() { updateAllHighlights(); }

void TextEdit::updateAllHighlights() {
  QList<QTextEdit::ExtraSelection> selections;

  applyScopeHighlights(selections);

  const int documentLength = document()->characterCount();

  for (const PendingEdit &edit : std::as_const(m_pendingEdits)) {
    QTextCharFormat format;

    format.setBackground(edit.accepted ? QColor(100, 200, 120, 55)
                                       : QColor(210, 100, 100, 45));

    const int start = qBound(0, edit.match.start, documentLength);

    if (edit.command.operation == EditCommand::Operation::Insert) {
      QTextEdit::ExtraSelection selection;

      if (edit.match.hasHighlightRange()) {
        const int highlightStart =
            qBound(0, edit.match.highlightStart, documentLength);

        const int highlightEnd =
            qBound(highlightStart, edit.match.highlightEnd, documentLength);

        QTextCursor cursor(document());
        cursor.setPosition(highlightStart);
        cursor.setPosition(highlightEnd, QTextCursor::KeepAnchor);

        selection.cursor = cursor;
      } else {
        QTextCursor cursor(document());
        cursor.setPosition(start);

        format.setProperty(QTextFormat::FullWidthSelection, true);

        selection.cursor = cursor;
      }

      selection.format = format;
      selections.append(selection);
      continue;
    }

    // Replace, ReplaceScope, Delete: tint the matched range.
    const int length = edit.match.end - edit.match.start;

    if (length <= 0 || start >= documentLength) {
      continue;
    }

    QTextCursor cursor(document());

    cursor.setPosition(start);
    cursor.movePosition(QTextCursor::Right, QTextCursor::KeepAnchor,
                        qMin(length, documentLength - start));

    QTextEdit::ExtraSelection selection;

    selection.cursor = cursor;
    selection.format = format;

    selections.append(selection);
  }

  setExtraSelections(selections);
}

void TextEdit::applyScopeHighlights(
    QList<QTextEdit::ExtraSelection> &selections) {
  if (m_highlightedScopes.isEmpty()) {
    return;
  }

  auto *doc = qobject_cast<TextDocument *>(document());

  if (!doc) {
    return;
  }

  doc->rebuildStructure();

  const DocumentStructure &structure = doc->structure();

  const QColor accent(137, 180, 250);

  QColor fullTint = accent;
  fullTint.setAlpha(40);

  QColor headingTint = accent;
  headingTint.setAlpha(15);

  const int documentLength = document()->characterCount();

  for (const QString &scopeId : std::as_const(m_highlightedScopes)) {
    const DocumentNode *node = structure.find(scopeId);

    if (!node) {
      continue;
    }

    if (m_highlightMode == HighlightMode::Intent ||
        m_highlightMode == HighlightMode::Both) {
      const int start = qBound(0, node->start, documentLength);
      const int end = qBound(start, node->end, documentLength);

      if (end > start) {
        QTextCursor cursor(document());
        cursor.setPosition(start);
        cursor.setPosition(end, QTextCursor::KeepAnchor);

        QTextEdit::ExtraSelection selection;
        selection.cursor = cursor;
        selection.format.setBackground(fullTint);

        selections.append(selection);
      }
    }

    if (m_highlightMode == HighlightMode::Sent ||
        m_highlightMode == HighlightMode::Both) {
      int headingStart = -1;
      int headingEnd = -1;

      if (structure.headingRange(scopeId, headingStart, headingEnd)) {
        const int start = qBound(0, headingStart, documentLength);
        const int end = qBound(start, headingEnd, documentLength);

        if (end > start) {
          QTextCursor cursor(document());
          cursor.setPosition(start);
          cursor.setPosition(end, QTextCursor::KeepAnchor);

          QTextEdit::ExtraSelection selection;
          selection.cursor = cursor;
          selection.format.setBackground(headingTint);

          selections.append(selection);
        }
      }
    }
  }
}

QString TextEdit::pendingEditSummary(const PendingEdit &edit) const {
  switch (edit.command.operation) {
  case EditCommand::Operation::Insert:
    return tr("Insert");

  case EditCommand::Operation::Replace:
    return tr("Replace \"%1\"").arg(edit.command.findString);

  case EditCommand::Operation::ReplaceScope:
    return tr("Replace entire body");

  case EditCommand::Operation::Delete:
    return tr("Delete \"%1\"").arg(edit.command.findString);
  }

  return tr("Edit");
}

QString TextEdit::pendingEditStatus(const PendingEdit &edit) const {
  return edit.accepted ? tr("Accepted") : tr("Rejected");
}

void TextEdit::toggleBold() {
  if (!m_delegate) {
    return;
  }

  QTextCursor cursor = textCursor();
  m_delegate->toggleBold(cursor);
  setTextCursor(cursor);
}

void TextEdit::toggleItalic() {
  if (!m_delegate) {
    return;
  }

  QTextCursor cursor = textCursor();
  m_delegate->toggleItalic(cursor);
  setTextCursor(cursor);
}

void TextEdit::toggleStrikethrough() {
  if (!m_delegate) {
    return;
  }

  QTextCursor cursor = textCursor();
  m_delegate->toggleStrikethrough(cursor);
  setTextCursor(cursor);
}

void TextEdit::toggleCodeSpan() {
  if (!m_delegate) {
    return;
  }

  QTextCursor cursor = textCursor();
  m_delegate->toggleCodeSpan(cursor);
  setTextCursor(cursor);
}

void TextEdit::toggleHighlight() {
  if (!m_delegate) {
    return;
  }

  QTextCursor cursor = textCursor();
  m_delegate->toggleHighlight(cursor);
  setTextCursor(cursor);
}

void TextEdit::insertLink() {
  if (!m_delegate) {
    return;
  }

  QTextCursor cursor = textCursor();
  m_delegate->insertLink(cursor);
  setTextCursor(cursor);
}

void TextEdit::insertWikiLink() {
  if (!m_delegate) {
    return;
  }

  QTextCursor cursor = textCursor();
  m_delegate->insertWikiLink(cursor);
  setTextCursor(cursor);
}

void TextEdit::insertAutolink() {
  if (!m_delegate) {
    return;
  }

  QTextCursor cursor = textCursor();
  m_delegate->insertAutolink(cursor);
  setTextCursor(cursor);
}

void TextEdit::insertImage() {
  if (!m_delegate) {
    return;
  }

  QTextCursor cursor = textCursor();
  m_delegate->insertImage(cursor);
  setTextCursor(cursor);
}

void TextEdit::insertMedia() {
  if (!m_delegate) {
    return;
  }

  QTextCursor cursor = textCursor();
  m_delegate->insertMedia(cursor);
  setTextCursor(cursor);
}

void TextEdit::setHeadingLevel(int level) {
  if (!m_delegate) {
    return;
  }

  QTextCursor cursor = textCursor();
  m_delegate->setHeadingLevel(cursor, level);
  setTextCursor(cursor);
}

void TextEdit::toggleBlockQuote() {
  if (!m_delegate) {
    return;
  }

  QTextCursor cursor = textCursor();
  m_delegate->toggleBlockQuote(cursor);
  setTextCursor(cursor);
}

void TextEdit::insertCallout(const QString &type) {
  if (!m_delegate) {
    return;
  }

  QTextCursor cursor = textCursor();
  m_delegate->insertCallout(cursor, type);
  setTextCursor(cursor);
}

void TextEdit::toggleBulletList() {
  if (!m_delegate) {
    return;
  }

  QTextCursor cursor = textCursor();
  m_delegate->toggleBulletList(cursor);
  setTextCursor(cursor);
}

void TextEdit::toggleOrderedList() {
  if (!m_delegate) {
    return;
  }

  QTextCursor cursor = textCursor();
  m_delegate->toggleOrderedList(cursor);
  setTextCursor(cursor);
}

void TextEdit::toggleTaskItem() {
  if (!m_delegate) {
    return;
  }

  QTextCursor cursor = textCursor();
  m_delegate->toggleTaskItem(cursor);
  setTextCursor(cursor);
}

void TextEdit::insertDefinitionList() {
  if (!m_delegate) {
    return;
  }

  QTextCursor cursor = textCursor();
  m_delegate->insertDefinitionList(cursor);
  setTextCursor(cursor);
}

void TextEdit::toggleCodeBlock() {
  if (!m_delegate) {
    return;
  }

  QTextCursor cursor = textCursor();
  m_delegate->toggleCodeBlock(cursor);
  setTextCursor(cursor);
}

void TextEdit::insertDiagramBlock(const QString &engine) {
  if (!m_delegate) {
    return;
  }

  QTextCursor cursor = textCursor();
  m_delegate->insertDiagramBlock(cursor, engine);
  setTextCursor(cursor);
}

void TextEdit::toggleMathBlock() {
  if (!m_delegate) {
    return;
  }

  QTextCursor cursor = textCursor();
  m_delegate->toggleMathBlock(cursor);
  setTextCursor(cursor);
}

void TextEdit::insertCollapsibleBlock() {
  if (!m_delegate) {
    return;
  }

  QTextCursor cursor = textCursor();
  m_delegate->insertCollapsibleBlock(cursor);
  setTextCursor(cursor);
}

void TextEdit::insertRawHtml() {
  if (!m_delegate) {
    return;
  }

  QTextCursor cursor = textCursor();
  m_delegate->insertRawHtml(cursor);
  setTextCursor(cursor);
}

void TextEdit::insertHorizontalRule() {
  if (!m_delegate) {
    return;
  }

  QTextCursor cursor = textCursor();
  m_delegate->insertHorizontalRule(cursor);
  setTextCursor(cursor);
}

void TextEdit::insertHardLineBreak() {
  if (!m_delegate) {
    return;
  }

  QTextCursor cursor = textCursor();
  m_delegate->insertHardLineBreak(cursor);
  setTextCursor(cursor);
}

void TextEdit::increaseIndent() {
  if (!m_delegate) {
    return;
  }

  QTextCursor cursor = textCursor();
  m_delegate->increaseIndent(cursor);
  setTextCursor(cursor);
}

void TextEdit::decreaseIndent() {
  if (!m_delegate) {
    return;
  }

  QTextCursor cursor = textCursor();
  m_delegate->decreaseIndent(cursor);
  setTextCursor(cursor);
}

void TextEdit::insertTable() {
  if (!m_delegate) {
    return;
  }

  QTextCursor cursor = textCursor();
  m_delegate->insertTable(cursor);
  setTextCursor(cursor);
}

void TextEdit::deleteTable() {
  if (!m_delegate) {
    return;
  }

  QTextCursor cursor = textCursor();
  m_delegate->deleteTable(cursor);
  setTextCursor(cursor);
}

void TextEdit::addTableRow() {
  if (!m_delegate) {
    return;
  }

  QTextCursor cursor = textCursor();
  m_delegate->addTableRow(cursor);
  setTextCursor(cursor);
}

void TextEdit::removeTableRow() {
  if (!m_delegate) {
    return;
  }

  QTextCursor cursor = textCursor();
  m_delegate->removeTableRow(cursor);
  setTextCursor(cursor);
}

void TextEdit::addTableColumn() {
  if (!m_delegate) {
    return;
  }

  QTextCursor cursor = textCursor();
  m_delegate->addTableColumn(cursor);
  setTextCursor(cursor);
}

void TextEdit::removeTableColumn() {
  if (!m_delegate) {
    return;
  }

  QTextCursor cursor = textCursor();
  m_delegate->removeTableColumn(cursor);
  setTextCursor(cursor);
}

void TextEdit::alignTableColumnLeft() {
  if (!m_delegate) {
    return;
  }

  QTextCursor cursor = textCursor();
  m_delegate->alignTableColumnLeft(cursor);
  setTextCursor(cursor);
}

void TextEdit::alignTableColumnCenter() {
  if (!m_delegate) {
    return;
  }

  QTextCursor cursor = textCursor();
  m_delegate->alignTableColumnCenter(cursor);
  setTextCursor(cursor);
}

void TextEdit::alignTableColumnRight() {
  if (!m_delegate) {
    return;
  }

  QTextCursor cursor = textCursor();
  m_delegate->alignTableColumnRight(cursor);
  setTextCursor(cursor);
}

void TextEdit::insertFootnote() {
  if (!m_delegate) {
    return;
  }

  QTextCursor cursor = textCursor();
  m_delegate->insertFootnote(cursor);
  setTextCursor(cursor);
}

void TextEdit::insertTag() {
  if (!m_delegate) {
    return;
  }

  QTextCursor cursor = textCursor();
  m_delegate->insertTag(cursor);
  setTextCursor(cursor);
}

void TextEdit::insertTableOfContents() {
  if (!m_delegate) {
    return;
  }

  QTextCursor cursor = textCursor();
  m_delegate->insertTableOfContents(cursor);
  setTextCursor(cursor);
}

void TextEdit::toggleFrontmatter() {
  if (!m_delegate) {
    return;
  }

  QTextCursor cursor = textCursor();
  m_delegate->toggleFrontmatter(cursor);
  setTextCursor(cursor);
}

void TextEdit::toggleSourceMode() {
  m_sourceMode = !m_sourceMode;
  emit formatChanged();
}

bool TextEdit::isBold() const {
  return m_delegate && m_delegate->isBold(textCursor());
}

bool TextEdit::isItalic() const {
  return m_delegate && m_delegate->isItalic(textCursor());
}

bool TextEdit::isStrikethrough() const {
  return m_delegate && m_delegate->isStrikethrough(textCursor());
}

bool TextEdit::isCodeSpan() const {
  return m_delegate && m_delegate->isCodeSpan(textCursor());
}

bool TextEdit::isHighlight() const {
  return m_delegate && m_delegate->isHighlight(textCursor());
}

bool TextEdit::isBlockQuote() const {
  return m_delegate && m_delegate->isBlockQuote(textCursor());
}

bool TextEdit::isBulletList() const {
  return m_delegate && m_delegate->isBulletList(textCursor());
}

bool TextEdit::isOrderedList() const {
  return m_delegate && m_delegate->isOrderedList(textCursor());
}

bool TextEdit::isTaskList() const {
  return m_delegate && m_delegate->isTaskList(textCursor());
}

bool TextEdit::isCodeBlock() const {
  return m_delegate && m_delegate->isCodeBlock(textCursor());
}

bool TextEdit::isMathBlock() const {
  return m_delegate && m_delegate->isMathBlock(textCursor());
}

int TextEdit::currentHeadingLevel() const {
  return m_delegate ? m_delegate->currentHeadingLevel(textCursor()) : 0;
}

bool TextEdit::isInsideTable() const {
  return m_delegate && m_delegate->isInsideTable(textCursor());
}

void TextEdit::resizeEvent(QResizeEvent *event) {
  QPlainTextEdit::resizeEvent(event);

  if (!m_toolbar || !m_reviewBar) {
    return;
  }

  const int toolbarHeight =
      m_toolbarVisible ? m_toolbar->sizeHint().height() : 0;

  m_toolbar->setGeometry(0, 0, width(), toolbarHeight);

  int reviewHeight = 0;

  if (m_reviewBar->isVisible()) {
    reviewHeight = qMin(m_reviewBar->sizeHint().height(),
                        height() / ReviewBarMaxHeightDivisor);
  }

  setViewportMargins(0, toolbarHeight, 0, reviewHeight);

  m_reviewBar->setGeometry(0, height() - reviewHeight, width(), reviewHeight);
}

void TextEdit::contextMenuEvent(QContextMenuEvent *event) {
  QMenu *menu = createStandardContextMenu();

  if (!menu) {
    menu = new QMenu(this);
  }

  menu->addSeparator();

  QMenu *format = buildFormatMenu(menu);
  menu->addMenu(format);

  menu->exec(event->globalPos());
  delete menu;
}

QMenu *TextEdit::buildFormatMenu(QWidget *parent) {
  auto *menu = new QMenu(tr("Format"), parent);

  if (!m_delegate) {
    QAction *disabled = menu->addAction(tr("(not available for this file type)"));
    disabled->setEnabled(false);
    return menu;
  }

  menu->addAction(tr("Bold"), this, &TextEdit::toggleBold);
  menu->addAction(tr("Italic"), this, &TextEdit::toggleItalic);
  menu->addAction(tr("Strikethrough"), this, &TextEdit::toggleStrikethrough);
  menu->addAction(tr("Inline Code"), this, &TextEdit::toggleCodeSpan);
  menu->addAction(tr("Highlight"), this, &TextEdit::toggleHighlight);

  menu->addSeparator();

  auto *headings = menu->addMenu(tr("Heading"));
  headings->addAction(tr("Paragraph"), this,
                      [this]() { setHeadingLevel(0); });
  for (int level = 1; level <= 6; ++level) {
    headings->addAction(tr("Heading %1").arg(level), this,
                        [this, level]() { setHeadingLevel(level); });
  }

  menu->addAction(tr("Bullet List"), this, &TextEdit::toggleBulletList);
  menu->addAction(tr("Numbered List"), this, &TextEdit::toggleOrderedList);
  menu->addAction(tr("Task Item"), this, &TextEdit::toggleTaskItem);
  menu->addAction(tr("Block Quote"), this, &TextEdit::toggleBlockQuote);

  menu->addSeparator();

  menu->addAction(tr("Code Block"), this, &TextEdit::toggleCodeBlock);
  menu->addAction(tr("Math Block"), this, &TextEdit::toggleMathBlock);
  menu->addAction(tr("Horizontal Rule"), this,
                  &TextEdit::insertHorizontalRule);
  menu->addAction(tr("Hard Line Break"), this, &TextEdit::insertHardLineBreak);

  menu->addSeparator();

  menu->addAction(tr("Insert Link"), this, &TextEdit::insertLink);
  menu->addAction(tr("Insert Image"), this, &TextEdit::insertImage);
  menu->addAction(tr("Insert Table"), this, &TextEdit::insertTable);
  menu->addAction(tr("Insert Footnote"), this, &TextEdit::insertFootnote);

  menu->addSeparator();

  menu->addAction(tr("Increase Indent"), this, &TextEdit::increaseIndent);
  menu->addAction(tr("Decrease Indent"), this, &TextEdit::decreaseIndent);

  return menu;
}