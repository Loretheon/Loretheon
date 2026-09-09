#include "HTMLFormatDelegate.h"
#include <QTextBlock>
#include <qregularexpression.h>

void HTMLFormatDelegate::wrapTag(QTextCursor &cursor, const QString &tag) {
  QString openTag = QString("<%1>").arg(tag);
  QString closeTag = QString("</%1>").arg(tag);
  QString selected = cursor.selectedText();

  if (selected.startsWith(openTag) && selected.endsWith(closeTag) &&
      selected.length() >= (openTag.length() + closeTag.length())) {
    cursor.insertText(
        selected.mid(openTag.length(),
                     selected.length() - openTag.length() - closeTag.length()));
  } else {
    cursor.insertText(openTag + selected + closeTag);
  }
}

// Inline Formats
void HTMLFormatDelegate::toggleBold(QTextCursor &cursor) {
  wrapTag(cursor, "strong");
}
void HTMLFormatDelegate::toggleItalic(QTextCursor &cursor) {
  wrapTag(cursor, "em");
}
void HTMLFormatDelegate::toggleStrikethrough(QTextCursor &cursor) {
  wrapTag(cursor, "s");
}
void HTMLFormatDelegate::toggleCodeSpan(QTextCursor &cursor) {
  wrapTag(cursor, "code");
}
void HTMLFormatDelegate::toggleHighlight(QTextCursor &cursor) {
  wrapTag(cursor, "mark");
}

// Links & Media
void HTMLFormatDelegate::insertLink(QTextCursor &cursor) {
  QString selected = cursor.selectedText();
  if (selected.startsWith("<a href=\"") && selected.endsWith("</a>")) {
    int contentStart = selected.indexOf("\">");
    if (contentStart != -1) {
      contentStart += 2;
      int contentLen = selected.length() - contentStart - 4;
      cursor.insertText(selected.mid(contentStart, contentLen));
      return;
    }
  }
  cursor.insertText(QString("<a href=\"url\">%1</a>").arg(selected));
}

void HTMLFormatDelegate::insertWikiLink(QTextCursor &cursor) {
  QString selected = cursor.selectedText();
  cursor.insertText(QString("<a href=\"%1\">%1</a>").arg(selected));
}

void HTMLFormatDelegate::insertAutolink(QTextCursor &cursor) {
  insertLink(cursor);
}

void HTMLFormatDelegate::insertImage(QTextCursor &cursor) {
  QString selected = cursor.selectedText();
  cursor.insertText(QString("<img src=\"url\" alt=\"%1\" />").arg(selected));
}

void HTMLFormatDelegate::insertMedia(QTextCursor &cursor) {
  QString selected = cursor.selectedText();
  cursor.insertText(QString("<video src=\"url\">%1</video>").arg(selected));
}

// Blocks
void HTMLFormatDelegate::setHeadingLevel(QTextCursor &cursor, int level) {
  cursor.movePosition(QTextCursor::StartOfLine);
  cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
  QString line = cursor.selectedText();

  for (int h = 1; h <= 6; ++h) {
    QString openH = QString("<h%1>").arg(h);
    QString closeH = QString("</h%1>").arg(h);
    if (line.startsWith(openH) && line.endsWith(closeH)) {
      line = line.mid(openH.length(),
                      line.length() - openH.length() - closeH.length());
      break;
    }
  }

  if (line.startsWith("<p>") && line.endsWith("</p>")) {
    line = line.mid(3, line.length() - 7);
  }

  if (level >= 1 && level <= 6) {
    cursor.insertText(QString("<h%1>%2</h%1>").arg(level).arg(line));
  } else {
    cursor.insertText(QString("<p>%1</p>").arg(line));
  }
}

void HTMLFormatDelegate::toggleBlockQuote(QTextCursor &cursor) {
  cursor.movePosition(QTextCursor::StartOfLine);
  cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
  QString line = cursor.selectedText();

  if (line.startsWith("<blockquote>") && line.endsWith("</blockquote>")) {
    cursor.insertText(line.mid(12, line.length() - 25));
  } else {
    cursor.insertText(QString("<blockquote>%1</blockquote>").arg(line));
  }
}

void HTMLFormatDelegate::insertCallout(QTextCursor &cursor,
                                       const QString &type) {
  cursor.movePosition(QTextCursor::StartOfLine);
  cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
  QString line = cursor.selectedText();

  cursor.insertText(QString("<div class=\"callout callout-%1\"><p>%2</p></div>")
                        .arg(type.toLower())
                        .arg(line));
}

void HTMLFormatDelegate::toggleBulletList(QTextCursor &cursor) {
  cursor.movePosition(QTextCursor::StartOfLine);
  cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
  QString line = cursor.selectedText();

  if (line.startsWith("<ul><li>") && line.endsWith("</li></ul>")) {
    cursor.insertText(line.mid(8, line.length() - 18));
  } else {
    cursor.insertText(QString("<ul><li>%1</li></ul>").arg(line));
  }
}

void HTMLFormatDelegate::toggleOrderedList(QTextCursor &cursor) {
  cursor.movePosition(QTextCursor::StartOfLine);
  cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
  QString line = cursor.selectedText();

  if (line.startsWith("<ol><li>") && line.endsWith("</li></ol>")) {
    cursor.insertText(line.mid(8, line.length() - 18));
  } else {
    cursor.insertText(QString("<ol><li>%1</li></ol>").arg(line));
  }
}

void HTMLFormatDelegate::toggleTaskItem(QTextCursor &cursor) {
  // Static instance avoids compiling the regex pattern repeatedly on every call
  static const QRegularExpression checkboxRegex(
      QStringLiteral("<input type=\"checkbox\"[^>]*>"));

  cursor.movePosition(QTextCursor::StartOfLine);
  cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
  QString line = cursor.selectedText();

  if (line.contains("<input type=\"checkbox\"")) {
    line.remove(checkboxRegex);
    cursor.insertText(line);
  } else {
    cursor.insertText(
        QString("<label><input type=\"checkbox\" /> %1</label>").arg(line));
  }
}

void HTMLFormatDelegate::insertDefinitionList(QTextCursor &cursor) {
  QString selected = cursor.selectedText();
  cursor.insertText(QString("<dl><dt>%1</dt><dd>Definition</dd></dl>")
                        .arg(selected.isEmpty() ? "Term" : selected));
}

void HTMLFormatDelegate::toggleCodeBlock(QTextCursor &cursor) {
  QString openTag = "<pre><code>\n";
  QString closeTag = "\n</code></pre>";
  QString selected = cursor.selectedText();

  if (selected.startsWith(openTag) && selected.endsWith(closeTag)) {
    cursor.insertText(
        selected.mid(openTag.length(),
                     selected.length() - openTag.length() - closeTag.length()));
  } else {
    cursor.insertText(openTag + selected + closeTag);
  }
}

void HTMLFormatDelegate::insertDiagramBlock(QTextCursor &cursor,
                                            const QString &engine) {
  QString selected = cursor.selectedText();
  cursor.insertText(
      QString("<div class=\"%1\">%2</div>").arg(engine).arg(selected));
}

void HTMLFormatDelegate::toggleMathBlock(QTextCursor &cursor) {
  wrapTag(cursor, "math");
}

void HTMLFormatDelegate::insertCollapsibleBlock(QTextCursor &cursor) {
  QString selected = cursor.selectedText();
  cursor.insertText(
      QString("<details><summary>Title</summary>%1</details>").arg(selected));
}

void HTMLFormatDelegate::insertRawHtml(QTextCursor &cursor) {
  wrapTag(cursor, "div");
}

void HTMLFormatDelegate::insertHorizontalRule(QTextCursor &cursor) {
  cursor.insertText("<hr />");
}

void HTMLFormatDelegate::insertHardLineBreak(QTextCursor &cursor) {
  cursor.insertText("<br />\n");
}

// Indentation
void HTMLFormatDelegate::increaseIndent(QTextCursor &cursor) {
  wrapTag(cursor, "blockquote");
}

void HTMLFormatDelegate::decreaseIndent(QTextCursor &cursor) {
  cursor.movePosition(QTextCursor::StartOfLine);
  cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
  QString line = cursor.selectedText();

  if (line.startsWith("<blockquote>") && line.endsWith("</blockquote>")) {
    cursor.insertText(line.mid(12, line.length() - 25));
  }
}

// Tables
void HTMLFormatDelegate::insertTable(QTextCursor &cursor) {
  cursor.insertText("<table>\n"
                    "  <thead>\n"
                    "    <tr><th>Header 1</th><th>Header 2</th></tr>\n"
                    "  </thead>\n"
                    "  <tbody>\n"
                    "    <tr><td>Cell 1</td><td>Cell 2</td></tr>\n"
                    "  </tbody>\n"
                    "</table>");
}

void HTMLFormatDelegate::deleteTable(QTextCursor &) {}
void HTMLFormatDelegate::addTableRow(QTextCursor &) {}
void HTMLFormatDelegate::removeTableRow(QTextCursor &) {}
void HTMLFormatDelegate::addTableColumn(QTextCursor &) {}
void HTMLFormatDelegate::removeTableColumn(QTextCursor &) {}
void HTMLFormatDelegate::alignTableColumnLeft(QTextCursor &) {}
void HTMLFormatDelegate::alignTableColumnCenter(QTextCursor &) {}
void HTMLFormatDelegate::alignTableColumnRight(QTextCursor &) {}

// Structure & Meta
void HTMLFormatDelegate::insertFootnote(QTextCursor &cursor) {
  QString selected = cursor.selectedText();
  cursor.insertText(QString("<sup><a href=\"#fn1\" id=\"ref1\">%1</a></sup>")
                        .arg(selected.isEmpty() ? "1" : selected));
}

void HTMLFormatDelegate::insertTag(QTextCursor &cursor) {
  wrapTag(cursor, "span class=\"tag\"");
}

void HTMLFormatDelegate::insertTableOfContents(QTextCursor &cursor) {
  cursor.insertText("<nav class=\"toc\"></nav>");
}

void HTMLFormatDelegate::toggleFrontmatter(QTextCursor &cursor) {
  cursor.insertText("<!--\ntitle: \ndate: \n-->\n");
}

// State Queries
bool HTMLFormatDelegate::isBold(const QTextCursor &cursor) const {
  QString selected = cursor.selectedText();
  return (selected.startsWith("<strong>") && selected.endsWith("</strong>")) ||
         (selected.startsWith("<b>") && selected.endsWith("</b>"));
}

bool HTMLFormatDelegate::isItalic(const QTextCursor &cursor) const {
  QString selected = cursor.selectedText();
  return (selected.startsWith("<em>") && selected.endsWith("</em>")) ||
         (selected.startsWith("<i>") && selected.endsWith("</i>"));
}

bool HTMLFormatDelegate::isStrikethrough(const QTextCursor &cursor) const {
  QString selected = cursor.selectedText();
  return (selected.startsWith("<s>") && selected.endsWith("</s>")) ||
         (selected.startsWith("<del>") && selected.endsWith("</del>"));
}

bool HTMLFormatDelegate::isCodeSpan(const QTextCursor &cursor) const {
  QString selected = cursor.selectedText();
  return selected.startsWith("<code>") && selected.endsWith("</code>");
}

bool HTMLFormatDelegate::isHighlight(const QTextCursor &cursor) const {
  QString selected = cursor.selectedText();
  return selected.startsWith("<mark>") && selected.endsWith("</mark>");
}

bool HTMLFormatDelegate::isBlockQuote(const QTextCursor &cursor) const {
  QString text = cursor.block().text().trimmed();
  return text.startsWith("<blockquote>");
}

bool HTMLFormatDelegate::isBulletList(const QTextCursor &cursor) const {
  QString text = cursor.block().text().trimmed();
  return text.startsWith("<ul>");
}

bool HTMLFormatDelegate::isOrderedList(const QTextCursor &cursor) const {
  QString text = cursor.block().text().trimmed();
  return text.startsWith("<ol>");
}

bool HTMLFormatDelegate::isTaskList(const QTextCursor &cursor) const {
  QString text = cursor.block().text().trimmed();
  return text.contains("type=\"checkbox\"");
}

bool HTMLFormatDelegate::isCodeBlock(const QTextCursor &cursor) const {
  QString text = cursor.block().text().trimmed();
  return text.startsWith("<pre>");
}

bool HTMLFormatDelegate::isMathBlock(const QTextCursor &cursor) const {
  QString text = cursor.block().text().trimmed();
  return text.startsWith("<math>");
}

int HTMLFormatDelegate::currentHeadingLevel(const QTextCursor &cursor) const {
  QString line = cursor.block().text().trimmed();
  for (int h = 1; h <= 6; ++h) {
    if (line.startsWith(QString("<h%1>").arg(h))) {
      return h;
    }
  }
  return 0;
}

bool HTMLFormatDelegate::isInsideTable(const QTextCursor &cursor) const {
  QString text = cursor.block().text();
  return text.contains("<td") || text.contains("<th") || text.contains("<tr");
}