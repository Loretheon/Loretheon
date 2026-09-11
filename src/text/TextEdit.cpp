#include "TextEdit.h"
#include <QMouseEvent>
#include <QToolTip>
#include "../../include/text/Toolbar.h"
#include "DocumentMode.h"
#include "HTMLFormatDelegate.h"
#include "MarkdownFormatDelegate.h"

#include <QCheckBox>
#include <QFile>
#include <QFileInfo>
#include <QFont>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QResizeEvent>
#include <QScrollArea>
#include <QSettings>
#include <QTextCharFormat>
#include <QTextCursor>
#include <QVBoxLayout>
#include <QWidget>

namespace {

constexpr auto kLastOpenedFileKey = "document/lastOpenedFile";

constexpr auto kAutoAcceptEditsKey = "editing/autoAcceptEdits";

} // namespace

TextEdit::TextEdit(QWidget *parent) : QPlainTextEdit(parent) {
  QSettings settings;
  setMouseTracking(true);
  m_autoAcceptEdits = settings.value(kAutoAcceptEditsKey, false).toBool();

  setupToolbar();
  setupReviewBar();

  setDocumentMode(DocumentMode::Markdown);

  connect(this, &QPlainTextEdit::cursorPositionChanged, this,
          &TextEdit::formatChanged);

  updateReviewBar();
}

const PendingEdit *TextEdit::pendingEditAtPosition(int position) const {
  for (const PendingEdit &edit : m_pendingEdits) {
    if (edit.match.isValid() &&
        position >= edit.match.start &&
        position <= edit.match.end) {
      return &edit;
    }
  }

  return nullptr;
}

QString TextEdit::pendingEditPreview(const PendingEdit &edit) const {
  QString before = edit.match.matchedText;

  switch (edit.command.operation) {
  case EditCommand::Operation::Insert:
    return before + "\n\n" + edit.generatedText;

  case EditCommand::Operation::Replace:
    return edit.generatedText;

  case EditCommand::Operation::Delete:
    return QString();

  }

  return {};
}

void TextEdit::mouseMoveEvent(QMouseEvent *event) {
  QPlainTextEdit::mouseMoveEvent(event);

  QTextCursor cursor = cursorForPosition(event->pos());

  const PendingEdit *edit =
      pendingEditAtPosition(cursor.position());

  if (!edit) {
    QToolTip::hideText();
    return;
  }

  const QString preview = pendingEditPreview(*edit);

  if (!preview.isEmpty()) {
    QToolTip::showText(
        event->globalPosition().toPoint(),
        preview,
        this);
  }
}

void TextEdit::setupToolbar() {
  m_toolbar = new Toolbar(this);

  setViewportMargins(0, m_toolbar->sizeHint().height(), 0, 0);

  setupToolbarConnections();
}

void TextEdit::setupToolbarConnections() {
  if (!m_toolbar) {
    return;
  }

  m_toolbar->setTextEdit(this);
}

void TextEdit::setupReviewBar() {
  m_reviewBar = new QWidget(this);

  m_reviewSummary = new QLabel(m_reviewBar);
  m_reviewSummary->setTextInteractionFlags(Qt::NoTextInteraction);

  m_acceptAllButton = new QPushButton(tr("Accept All"), m_reviewBar);
  m_rejectAllButton = new QPushButton(tr("Reject All"), m_reviewBar);
  m_autoAcceptCheckBox = new QCheckBox(tr("Auto-accept"), m_reviewBar);
  m_autoAcceptCheckBox->setChecked(m_autoAcceptEdits);

  m_reviewScrollArea = new QScrollArea(m_reviewBar);
  m_reviewScrollArea->setWidgetResizable(true);
  m_reviewScrollArea->setFrameShape(QFrame::NoFrame);

  m_reviewContent = new QWidget;
  m_reviewLayout = new QVBoxLayout(m_reviewContent);
  m_reviewLayout->setContentsMargins(0, 0, 0, 0);
  m_reviewLayout->setSpacing(4);

  m_reviewScrollArea->setWidget(m_reviewContent);

  auto *buttonLayout = new QHBoxLayout;
  buttonLayout->setContentsMargins(0, 0, 0, 0);
  buttonLayout->addWidget(m_reviewSummary);
  buttonLayout->addStretch();
  buttonLayout->addWidget(m_acceptAllButton);
  buttonLayout->addWidget(m_rejectAllButton);
  buttonLayout->addWidget(m_autoAcceptCheckBox);

  auto *layout = new QVBoxLayout(m_reviewBar);
  layout->setContentsMargins(8, 6, 8, 6);
  layout->setSpacing(6);
  layout->addLayout(buttonLayout);
  layout->addWidget(m_reviewScrollArea);

  connect(m_acceptAllButton, &QPushButton::clicked, this,
          &TextEdit::acceptAllPendingEditsRequested);

  connect(m_rejectAllButton, &QPushButton::clicked, this,
          &TextEdit::rejectAllPendingEditsRequested);

  connect(m_autoAcceptCheckBox, &QCheckBox::toggled, this,
          &TextEdit::setAutoAcceptEdits);

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
    m_delegate = nullptr;
    break;
  }

  emit formatChanged();
}

bool TextEdit::openFile(const QString &filePath) {
  QFile file(filePath);

  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return false;
  }

  QTextStream in(&file);

  setPlainText(in.readAll());

  file.close();

  QFileInfo info(filePath);

  const QString suffix = info.suffix().toLower();

  if (suffix == "md" || suffix == "markdown" || suffix == "mdown" ||
      suffix == "mkd") {

    setDocumentMode(DocumentMode::Markdown);

  } else if (suffix == "html" || suffix == "htm" || suffix == "xhtml") {

    setDocumentMode(DocumentMode::Html);

  } else {

    setDocumentMode(DocumentMode::PlainText);
  }

  QSettings settings;

  settings.setValue(kLastOpenedFileKey, QFileInfo(filePath).absoluteFilePath());

  clearPendingEdits();

  return true;
}

QString TextEdit::lastOpenedFile() {
  QSettings settings;

  return settings.value(kLastOpenedFileKey).toString();
}

void TextEdit::setAutoAcceptEdits(bool enabled) {
  if (m_autoAcceptEdits == enabled) {
    return;
  }

  m_autoAcceptEdits = enabled;

  QSettings settings;

  settings.setValue(kAutoAcceptEditsKey, enabled);

  emit autoAcceptChanged(enabled);

  updateReviewBar();
}

void TextEdit::setPendingEditAccepted(int editId, bool accepted) {
  auto it = m_pendingEdits.find(editId);

  if (it == m_pendingEdits.end()) {
    return;
  }

  it->accepted = accepted;

  updateReviewBar();
  updatePendingHighlight();

  if (accepted) {
    emit acceptPendingEditRequested(editId);
  } else {
    emit rejectPendingEditRequested(editId);
  }
}

void TextEdit::showPendingEdit(const PendingEdit &edit) {
  m_pendingEdits.insert(edit.id, edit);

  updatePendingHighlight();
  updateReviewBar();
}

void TextEdit::updatePendingEdit(const PendingEdit &edit) {
  m_pendingEdits.insert(edit.id, edit);

  updatePendingHighlight();
  updateReviewBar();
}

void TextEdit::removePendingEdit(int editId) {
  m_pendingEdits.remove(editId);

  updatePendingHighlight();
  updateReviewBar();
}

void TextEdit::clearPendingEdits() {
  m_pendingEdits.clear();

  updatePendingHighlight();
  updateReviewBar();
}

void TextEdit::refreshPendingEdits() {
  updatePendingHighlight();
  updateReviewBar();
}

void TextEdit::rebuildReviewRows() {
  if (!m_reviewLayout) {
    return;
  }

  while (m_reviewLayout->count() > 0) {
    QLayoutItem *item = m_reviewLayout->takeAt(0);

    if (!item) {
      continue;
    }

    if (QWidget *widget = item->widget()) {
      widget->deleteLater();
    }

    delete item;
  }

  QList<int> ids = m_pendingEdits.keys();

  std::sort(ids.begin(), ids.end());

  for (const int id : ids) {
    const PendingEdit edit = m_pendingEdits.value(id);
    m_reviewLayout->addWidget(createReviewRow(edit));
  }
}

QWidget *TextEdit::createReviewRow(const PendingEdit &edit) {
  auto *row = new QWidget(m_reviewContent);

  setReviewRowStyle(row, edit);

  auto *title = new QLabel(row);
  title->setText(tr("Edit %1 — %2").arg(edit.id).arg(pendingEditSummary(edit)));

  auto *preview = new QLabel(row);
  preview->setWordWrap(true);

  if (edit.command.operation == EditCommand::Operation::Delete) {
    preview->setText(edit.command.findString.isEmpty()
                         ? tr("Delete content")
                         : edit.command.findString);
  } else if (!edit.generatedText.isEmpty()) {
    preview->setText(edit.generatedText);
  } else {
    preview->setText(tr("Generating…"));
  }

  auto *acceptButton = new QPushButton(tr("Accept"), row);
  auto *rejectButton = new QPushButton(tr("Reject"), row);

  connect(
      acceptButton, &QPushButton::clicked, this,
      [this, editId = edit.id]() { emit acceptPendingEditRequested(editId); });

  connect(
      rejectButton, &QPushButton::clicked, this,
      [this, editId = edit.id]() { emit rejectPendingEditRequested(editId); });

  auto *textLayout = new QVBoxLayout;
  textLayout->setContentsMargins(0, 0, 0, 0);
  textLayout->addWidget(title);
  textLayout->addWidget(preview);

  auto *layout = new QHBoxLayout(row);
  layout->setContentsMargins(6, 5, 6, 5);
  layout->setSpacing(6);
  layout->addLayout(textLayout, 1);

  if (edit.accepted) {
    acceptButton->setEnabled(false);
    rejectButton->setEnabled(true);
  } else {
    acceptButton->setEnabled(true);
    rejectButton->setEnabled(false);
  }

  layout->addWidget(acceptButton);
  layout->addWidget(rejectButton);

  return row;
}

void TextEdit::setReviewRowStyle(QWidget *row, const PendingEdit &edit) {
  Q_UNUSED(row);
  Q_UNUSED(edit);
  // Custom styling removed to allow native widget drawing
}

void TextEdit::updateReviewBar() {
  if (!m_reviewBar) {
    return;
  }

  if (m_pendingEdits.isEmpty()) {
    m_reviewBar->hide();

    const QList<QAbstractTextDocumentLayout::PaintContext> unused;
    Q_UNUSED(unused);

    // Trigger resize to restore margins
    QResizeEvent event(size(), size());
    resizeEvent(&event);
    return;
  }

  rebuildReviewRows();

  int acceptedCount = 0;

  for (const PendingEdit &edit : std::as_const(m_pendingEdits)) {
    if (edit.accepted) {
      ++acceptedCount;
    }
  }

  m_reviewSummary->setText(tr("%1 edits pending · %2 selected")
                               .arg(m_pendingEdits.size())
                               .arg(acceptedCount));

  m_autoAcceptCheckBox->setChecked(m_autoAcceptEdits);

  m_reviewBar->show();

  // Trigger geometry/margin recalculation on visible bar
  QResizeEvent event(size(), size());
  resizeEvent(&event);
}

void TextEdit::updatePendingHighlight() {
  QList<QTextEdit::ExtraSelection> selections;

  for (const PendingEdit &edit : std::as_const(m_pendingEdits)) {
    const int start =
        qBound(0, edit.match.start, document()->characterCount());

    QTextCursor cursor(document());
    cursor.setPosition(start);

    QTextCharFormat format;

    if (edit.accepted) {
      format.setBackground(QColor(100, 200, 120, 55));
    } else {
      format.setBackground(QColor(210, 100, 100, 45));
    }

    if (edit.command.operation == EditCommand::Operation::Insert) {
      // Highlight the insertion line without selecting any existing text.
      format.setProperty(QTextFormat::FullWidthSelection, true);

      QTextEdit::ExtraSelection selection;
      selection.cursor = cursor;
      selection.format = format;

      selections.append(selection);

      continue;
    }

    const int length = edit.command.findString.length();

    if (length <= 0) {
      continue;
    }

    const int available = document()->characterCount() - start;

    cursor.movePosition(QTextCursor::Right,
                        QTextCursor::KeepAnchor,
                        qMin(length, available));

    QTextEdit::ExtraSelection selection;
    selection.cursor = cursor;
    selection.format = format;

    selections.append(selection);
  }

  setExtraSelections(selections);
}

QString TextEdit::pendingEditSummary(const PendingEdit &edit) const {
  switch (edit.command.operation) {
  case EditCommand::Operation::Insert:
    return tr("Insert");

  case EditCommand::Operation::Replace:
    return tr("Replace \"%1\"").arg(edit.command.findString);

  case EditCommand::Operation::Delete:
    return tr("Delete \"%1\"").arg(edit.command.findString);
  }

  return tr("Edit");
}

QString TextEdit::pendingEditStatus(const PendingEdit &edit) const {
  return edit.accepted ? tr("Accepted") : tr("Rejected");
}

void TextEdit::toggleBold() {
  if (m_delegate) {
    QTextCursor c = textCursor();

    m_delegate->toggleBold(c);

    setTextCursor(c);
  }
}

void TextEdit::toggleItalic() {
  if (m_delegate) {
    QTextCursor c = textCursor();

    m_delegate->toggleItalic(c);

    setTextCursor(c);
  }
}

void TextEdit::toggleStrikethrough() {
  if (m_delegate) {
    QTextCursor c = textCursor();

    m_delegate->toggleStrikethrough(c);

    setTextCursor(c);
  }
}

void TextEdit::toggleCodeSpan() {
  if (m_delegate) {
    QTextCursor c = textCursor();

    m_delegate->toggleCodeSpan(c);

    setTextCursor(c);
  }
}

void TextEdit::toggleHighlight() {
  if (m_delegate) {
    QTextCursor c = textCursor();

    m_delegate->toggleHighlight(c);

    setTextCursor(c);
  }
}

void TextEdit::insertLink() {
  if (m_delegate) {
    QTextCursor c = textCursor();

    m_delegate->insertLink(c);

    setTextCursor(c);
  }
}

void TextEdit::insertWikiLink() {
  if (m_delegate) {
    QTextCursor c = textCursor();

    m_delegate->insertWikiLink(c);

    setTextCursor(c);
  }
}

void TextEdit::insertAutolink() {
  if (m_delegate) {
    QTextCursor c = textCursor();

    m_delegate->insertAutolink(c);

    setTextCursor(c);
  }
}

void TextEdit::insertImage() {
  if (m_delegate) {
    QTextCursor c = textCursor();

    m_delegate->insertImage(c);

    setTextCursor(c);
  }
}

void TextEdit::insertMedia() {
  if (m_delegate) {
    QTextCursor c = textCursor();

    m_delegate->insertMedia(c);

    setTextCursor(c);
  }
}

void TextEdit::setHeadingLevel(int level) {
  if (m_delegate) {
    QTextCursor c = textCursor();

    m_delegate->setHeadingLevel(c, level);

    setTextCursor(c);
  }
}

void TextEdit::toggleBlockQuote() {
  if (m_delegate) {
    QTextCursor c = textCursor();

    m_delegate->toggleBlockQuote(c);

    setTextCursor(c);
  }
}

void TextEdit::insertCallout(const QString &type) {
  if (m_delegate) {
    QTextCursor c = textCursor();

    m_delegate->insertCallout(c, type);

    setTextCursor(c);
  }
}

void TextEdit::toggleBulletList() {
  if (m_delegate) {
    QTextCursor c = textCursor();

    m_delegate->toggleBulletList(c);

    setTextCursor(c);
  }
}

void TextEdit::toggleOrderedList() {
  if (m_delegate) {
    QTextCursor c = textCursor();

    m_delegate->toggleOrderedList(c);

    setTextCursor(c);
  }
}

void TextEdit::toggleTaskItem() {
  if (m_delegate) {
    QTextCursor c = textCursor();

    m_delegate->toggleTaskItem(c);

    setTextCursor(c);
  }
}

void TextEdit::insertDefinitionList() {
  if (m_delegate) {
    QTextCursor c = textCursor();

    m_delegate->insertDefinitionList(c);

    setTextCursor(c);
  }
}

void TextEdit::toggleCodeBlock() {
  if (m_delegate) {
    QTextCursor c = textCursor();

    m_delegate->toggleCodeBlock(c);

    setTextCursor(c);
  }
}

void TextEdit::insertDiagramBlock(const QString &engine) {
  if (m_delegate) {
    QTextCursor c = textCursor();

    m_delegate->insertDiagramBlock(c, engine);

    setTextCursor(c);
  }
}

void TextEdit::toggleMathBlock() {
  if (m_delegate) {
    QTextCursor c = textCursor();

    m_delegate->toggleMathBlock(c);

    setTextCursor(c);
  }
}

void TextEdit::insertCollapsibleBlock() {
  if (m_delegate) {
    QTextCursor c = textCursor();

    m_delegate->insertCollapsibleBlock(c);

    setTextCursor(c);
  }
}

void TextEdit::insertRawHtml() {
  if (m_delegate) {
    QTextCursor c = textCursor();

    m_delegate->insertRawHtml(c);

    setTextCursor(c);
  }
}

void TextEdit::insertHorizontalRule() {
  if (m_delegate) {
    QTextCursor c = textCursor();

    m_delegate->insertHorizontalRule(c);

    setTextCursor(c);
  }
}

void TextEdit::insertHardLineBreak() {
  if (m_delegate) {
    QTextCursor c = textCursor();

    m_delegate->insertHardLineBreak(c);

    setTextCursor(c);
  }
}

void TextEdit::increaseIndent() {
  if (m_delegate) {
    QTextCursor c = textCursor();

    m_delegate->increaseIndent(c);

    setTextCursor(c);
  }
}

void TextEdit::decreaseIndent() {
  if (m_delegate) {
    QTextCursor c = textCursor();

    m_delegate->decreaseIndent(c);

    setTextCursor(c);
  }
}

void TextEdit::insertTable() {
  if (m_delegate) {
    QTextCursor c = textCursor();

    m_delegate->insertTable(c);

    setTextCursor(c);
  }
}

void TextEdit::deleteTable() {
  if (m_delegate) {
    QTextCursor c = textCursor();

    m_delegate->deleteTable(c);

    setTextCursor(c);
  }
}

void TextEdit::addTableRow() {
  if (m_delegate) {
    QTextCursor c = textCursor();

    m_delegate->addTableRow(c);

    setTextCursor(c);
  }
}

void TextEdit::removeTableRow() {
  if (m_delegate) {
    QTextCursor c = textCursor();

    m_delegate->removeTableRow(c);

    setTextCursor(c);
  }
}

void TextEdit::addTableColumn() {
  if (m_delegate) {
    QTextCursor c = textCursor();

    m_delegate->addTableColumn(c);

    setTextCursor(c);
  }
}

void TextEdit::removeTableColumn() {
  if (m_delegate) {
    QTextCursor c = textCursor();

    m_delegate->removeTableColumn(c);

    setTextCursor(c);
  }
}

void TextEdit::alignTableColumnLeft() {
  if (m_delegate) {
    QTextCursor c = textCursor();

    m_delegate->alignTableColumnLeft(c);

    setTextCursor(c);
  }
}

void TextEdit::alignTableColumnCenter() {
  if (m_delegate) {
    QTextCursor c = textCursor();

    m_delegate->alignTableColumnCenter(c);

    setTextCursor(c);
  }
}

void TextEdit::alignTableColumnRight() {
  if (m_delegate) {
    QTextCursor c = textCursor();

    m_delegate->alignTableColumnRight(c);

    setTextCursor(c);
  }
}

void TextEdit::insertFootnote() {
  if (m_delegate) {
    QTextCursor c = textCursor();

    m_delegate->insertFootnote(c);

    setTextCursor(c);
  }
}

void TextEdit::insertTag() {
  if (m_delegate) {
    QTextCursor c = textCursor();

    m_delegate->insertTag(c);

    setTextCursor(c);
  }
}

void TextEdit::insertTableOfContents() {
  if (m_delegate) {
    QTextCursor c = textCursor();

    m_delegate->insertTableOfContents(c);

    setTextCursor(c);
  }
}

void TextEdit::toggleFrontmatter() {
  if (m_delegate) {
    QTextCursor c = textCursor();

    m_delegate->toggleFrontmatter(c);

    setTextCursor(c);
  }
}

void TextEdit::toggleSourceMode() {
  m_sourceMode = !m_sourceMode;

  emit formatChanged();
}

bool TextEdit::isBold() const {
  return m_delegate ? m_delegate->isBold(textCursor()) : false;
}

bool TextEdit::isItalic() const {
  return m_delegate ? m_delegate->isItalic(textCursor()) : false;
}

bool TextEdit::isStrikethrough() const {
  return m_delegate ? m_delegate->isStrikethrough(textCursor()) : false;
}

bool TextEdit::isCodeSpan() const {
  return m_delegate ? m_delegate->isCodeSpan(textCursor()) : false;
}

bool TextEdit::isHighlight() const {
  return m_delegate ? m_delegate->isHighlight(textCursor()) : false;
}

bool TextEdit::isBlockQuote() const {
  return m_delegate ? m_delegate->isBlockQuote(textCursor()) : false;
}

bool TextEdit::isBulletList() const {
  return m_delegate ? m_delegate->isBulletList(textCursor()) : false;
}

bool TextEdit::isOrderedList() const {
  return m_delegate ? m_delegate->isOrderedList(textCursor()) : false;
}

bool TextEdit::isTaskList() const {
  return m_delegate ? m_delegate->isTaskList(textCursor()) : false;
}

bool TextEdit::isCodeBlock() const {
  return m_delegate ? m_delegate->isCodeBlock(textCursor()) : false;
}

bool TextEdit::isMathBlock() const {
  return m_delegate ? m_delegate->isMathBlock(textCursor()) : false;
}

int TextEdit::currentHeadingLevel() const {
  return m_delegate ? m_delegate->currentHeadingLevel(textCursor()) : 0;
}

bool TextEdit::isInsideTable() const {
  return m_delegate ? m_delegate->isInsideTable(textCursor()) : false;
}

void TextEdit::resizeEvent(QResizeEvent *event) {
  QPlainTextEdit::resizeEvent(event);

  if (!m_reviewBar || !m_toolbar) {
    return;
  }

  const int toolbarHeight = m_toolbar->sizeHint().height();

  int reviewHeight = 0;
  if (m_reviewBar->isVisible()) {
    reviewHeight = m_reviewBar->sizeHint().height();
    // Cap height at 1/3rd of the viewport if long content is scrollable
    reviewHeight = qMin(reviewHeight, height() / 3);
  }

  setViewportMargins(0, toolbarHeight, 0, reviewHeight);

  m_reviewBar->setGeometry(0, height() - reviewHeight, width(), reviewHeight);
}