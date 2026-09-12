#include "../../../include/text/formats/MarkdownFormatDelegate.h"
#include <QTextBlock>

void MarkdownFormatDelegate::wrapSelection(QTextCursor &cursor,
                                           const QString &prefix,
                                           const QString &suffix) {
  QString selected = cursor.selectedText();
  if (selected.startsWith(prefix) && selected.endsWith(suffix) &&
      selected.length() >= (prefix.length() + suffix.length())) {
    cursor.insertText(
        selected.mid(prefix.length(),
                     selected.length() - prefix.length() - suffix.length()));
  } else {
    cursor.insertText(prefix + selected + suffix);
  }
}

void MarkdownFormatDelegate::toggleLinePrefix(QTextCursor &cursor,
                                              const QString &prefix) {
  cursor.movePosition(QTextCursor::StartOfLine);
  cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
  QString line = cursor.selectedText();
  if (line.startsWith(prefix)) {
    cursor.insertText(line.mid(prefix.length()));
  } else {
    cursor.insertText(prefix + line);
  }
}

void MarkdownFormatDelegate::toggleBold(QTextCursor &cursor) {
  wrapSelection(cursor, "**", "**");
}
void MarkdownFormatDelegate::toggleItalic(QTextCursor &cursor) {
  wrapSelection(cursor, "*", "*");
}
void MarkdownFormatDelegate::toggleStrikethrough(QTextCursor &cursor) {
  wrapSelection(cursor, "~~", "~~");
}
void MarkdownFormatDelegate::toggleCodeSpan(QTextCursor &cursor) {
  wrapSelection(cursor, "`", "`");
}
void MarkdownFormatDelegate::toggleHighlight(QTextCursor &cursor) {
  wrapSelection(cursor, "==", "==");
}

void MarkdownFormatDelegate::insertLink(QTextCursor &cursor) {
  wrapSelection(cursor, "[", "](url)");
}
void MarkdownFormatDelegate::insertWikiLink(QTextCursor &cursor) {
  wrapSelection(cursor, "[[", "]]");
}
void MarkdownFormatDelegate::insertAutolink(QTextCursor &cursor) {
  wrapSelection(cursor, "<", ">");
}
void MarkdownFormatDelegate::insertImage(QTextCursor &cursor) {
  wrapSelection(cursor, "![alt](", ")");
}
void MarkdownFormatDelegate::insertMedia(QTextCursor &cursor) {
  cursor.insertText("<video src=\"url\"></video>");
}

void MarkdownFormatDelegate::setHeadingLevel(QTextCursor &cursor, int level) {
  cursor.movePosition(QTextCursor::StartOfLine);
  cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
  QString line = cursor.selectedText();

  int existingHashes = 0;
  while (existingHashes < line.length() && line[existingHashes] == '#') {
    existingHashes++;
  }
  if (existingHashes < line.length() && line[existingHashes] == ' ') {
    existingHashes++;
  }

  QString cleanLine = line.mid(existingHashes);
  if (level > 0) {
    cursor.insertText(QString("#").repeated(level) + " " + cleanLine);
  } else {
    cursor.insertText(cleanLine);
  }
}

void MarkdownFormatDelegate::toggleBlockQuote(QTextCursor &cursor) {
  toggleLinePrefix(cursor, "> ");
}
void MarkdownFormatDelegate::insertCallout(QTextCursor &cursor,
                                           const QString &type) {
  toggleLinePrefix(cursor, "> [!" + type + "]\n> ");
}
void MarkdownFormatDelegate::toggleBulletList(QTextCursor &cursor) {
  toggleLinePrefix(cursor, "- ");
}
void MarkdownFormatDelegate::toggleOrderedList(QTextCursor &cursor) {
  toggleLinePrefix(cursor, "1. ");
}
void MarkdownFormatDelegate::toggleTaskItem(QTextCursor &cursor) {
  toggleLinePrefix(cursor, "- [ ] ");
}
void MarkdownFormatDelegate::insertDefinitionList(QTextCursor &cursor) {
  cursor.insertText("\nTerm\n: Definition\n");
}
void MarkdownFormatDelegate::toggleCodeBlock(QTextCursor &cursor) {
  wrapSelection(cursor, "```\n", "\n```");
}
void MarkdownFormatDelegate::insertDiagramBlock(QTextCursor &cursor,
                                                const QString &engine) {
  wrapSelection(cursor, "```" + engine + "\n", "\n```");
}
void MarkdownFormatDelegate::toggleMathBlock(QTextCursor &cursor) {
  wrapSelection(cursor, "$$\n", "\n$$");
}
void MarkdownFormatDelegate::insertCollapsibleBlock(QTextCursor &cursor) {
  cursor.insertText(
      "<details>\n<summary>Title</summary>\n\nContent\n</details>");
}
void MarkdownFormatDelegate::insertRawHtml(QTextCursor &cursor) {
  wrapSelection(cursor, "<div>", "</div>");
}
void MarkdownFormatDelegate::insertHorizontalRule(QTextCursor &cursor) {
  cursor.insertText("\n---\n");
}
void MarkdownFormatDelegate::insertHardLineBreak(QTextCursor &cursor) {
  cursor.insertText("  \n");
}

void MarkdownFormatDelegate::increaseIndent(QTextCursor &cursor) {
  toggleLinePrefix(cursor, "  ");
}
void MarkdownFormatDelegate::decreaseIndent(QTextCursor &cursor) {}

void MarkdownFormatDelegate::insertTable(QTextCursor &cursor) {
  cursor.insertText(
      "| Header 1 | Header 2 |\n| --- | --- |\n| Cell 1 | Cell 2 |\n");
}
void MarkdownFormatDelegate::deleteTable(QTextCursor &cursor) {}
void MarkdownFormatDelegate::addTableRow(QTextCursor &cursor) {}
void MarkdownFormatDelegate::removeTableRow(QTextCursor &cursor) {}
void MarkdownFormatDelegate::addTableColumn(QTextCursor &cursor) {}
void MarkdownFormatDelegate::removeTableColumn(QTextCursor &cursor) {}
void MarkdownFormatDelegate::alignTableColumnLeft(QTextCursor &cursor) {}
void MarkdownFormatDelegate::alignTableColumnCenter(QTextCursor &cursor) {}
void MarkdownFormatDelegate::alignTableColumnRight(QTextCursor &cursor) {}

void MarkdownFormatDelegate::insertFootnote(QTextCursor &cursor) {
  wrapSelection(cursor, "[^1]", "\n\n[^1]: Footnote content");
}
void MarkdownFormatDelegate::insertTag(QTextCursor &cursor) {
  wrapSelection(cursor, "#", "");
}
void MarkdownFormatDelegate::insertTableOfContents(QTextCursor &cursor) {
  cursor.insertText("\n[TOC]\n");
}
void MarkdownFormatDelegate::toggleFrontmatter(QTextCursor &cursor) {
  cursor.insertText("---\ntitle: \ndate: \n---\n");
}

bool MarkdownFormatDelegate::isBold(const QTextCursor &cursor) const {
  QString selected = cursor.selectedText();
  return selected.startsWith("**") && selected.endsWith("**");
}

bool MarkdownFormatDelegate::isItalic(const QTextCursor &cursor) const {
  QString selected = cursor.selectedText();
  return selected.startsWith("*") && selected.endsWith("*");
}

bool MarkdownFormatDelegate::isStrikethrough(const QTextCursor &cursor) const {
  QString selected = cursor.selectedText();
  return selected.startsWith("~~") && selected.endsWith("~~");
}

bool MarkdownFormatDelegate::isCodeSpan(const QTextCursor &cursor) const {
  QString selected = cursor.selectedText();
  return selected.startsWith("`") && selected.endsWith("`");
}

bool MarkdownFormatDelegate::isHighlight(const QTextCursor &cursor) const {
  QString selected = cursor.selectedText();
  return selected.startsWith("==") && selected.endsWith("==");
}

bool MarkdownFormatDelegate::isBlockQuote(const QTextCursor &cursor) const {
  return cursor.block().text().trimmed().startsWith(">");
}

bool MarkdownFormatDelegate::isBulletList(const QTextCursor &cursor) const {
  QString text = cursor.block().text().trimmed();
  return text.startsWith("- ") || text.startsWith("* ") ||
         text.startsWith("+ ");
}

bool MarkdownFormatDelegate::isOrderedList(const QTextCursor &cursor) const {
  QString text = cursor.block().text().trimmed();
  return !text.isEmpty() && text[0].isDigit();
}

bool MarkdownFormatDelegate::isTaskList(const QTextCursor &cursor) const {
  QString text = cursor.block().text().trimmed();
  return text.startsWith("- [ ]") || text.startsWith("- [x]");
}

bool MarkdownFormatDelegate::isCodeBlock(const QTextCursor &cursor) const {
  return cursor.block().text().trimmed().startsWith("```");
}

bool MarkdownFormatDelegate::isMathBlock(const QTextCursor &cursor) const {
  return cursor.block().text().trimmed().startsWith("$$");
}

int MarkdownFormatDelegate::currentHeadingLevel(
    const QTextCursor &cursor) const {
  QString line = cursor.block().text();
  int count = 0;
  while (count < line.length() && line[count] == '#')
    count++;
  return (count > 0 && count <= 6 && line.length() > count &&
          line[count] == ' ')
             ? count
             : 0;
}

bool MarkdownFormatDelegate::isInsideTable(const QTextCursor &cursor) const {
  return cursor.block().text().contains('|');
}