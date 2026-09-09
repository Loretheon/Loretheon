#include "TextEdit.h"

#include "../../include/text/Toolbar.h"
#include "DocumentMode.h"
#include "HTMLFormatDelegate.h"
#include "MarkdownFormatDelegate.h"
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

TextEdit::TextEdit(QWidget *parent) : QPlainTextEdit(parent) {
  setupToolbar();
  setDocumentMode(DocumentMode::Markdown);
  connect(this, &QPlainTextEdit::cursorPositionChanged, this,
          &TextEdit::formatChanged);
}

void TextEdit::setupToolbar() {
  m_toolbar = new Toolbar(this);

  // Reserve top viewport margin to fit the overlay toolbar without covering
  // text
  setViewportMargins(0, m_toolbar->sizeHint().height(), 0, 0);

  setupToolbarConnections();
}

void TextEdit::setupToolbarConnections() {
  if (!m_toolbar)
    return;

  // Delegate action setup and state synchronization to Toolbar::setTextEdit
  m_toolbar->setTextEdit(this);
}

void TextEdit::setDocumentMode(DocumentMode mode) {
  if (m_mode == mode && m_delegate)
    return;

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
  QString suffix = info.suffix().toLower();

  if (suffix == "md" || suffix == "markdown" || suffix == "mdown" ||
      suffix == "mkd") {
    setDocumentMode(DocumentMode::Markdown);
  } else if (suffix == "html" || suffix == "htm" || suffix == "xhtml") {
    setDocumentMode(DocumentMode::Html);
  } else {
    setDocumentMode(DocumentMode::PlainText);
  }

  return true;
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