#include "../../../include/text/formats/HTMLFormatDelegate.h"

#include <QRegularExpression>
#include <QTextBlock>
#include <QTextCursor>

namespace {

struct TagPair {
  QString open;
  QString close;
};

TagPair tagPair(const QString &tag) {
  return {QStringLiteral("<%1>").arg(tag), QStringLiteral("</%1>").arg(tag)};
}

QString stripPair(const QString &text, const TagPair &tags) {
  if (!text.startsWith(tags.open) || !text.endsWith(tags.close) ||
      text.size() < tags.open.size() + tags.close.size()) {
    return text;
  }

  return text.mid(tags.open.size(),
                  text.size() - tags.open.size() - tags.close.size());
}

void selectLine(QTextCursor &cursor) {
  cursor.movePosition(QTextCursor::StartOfLine);
  cursor.movePosition(QTextCursor::EndOfLine, QTextCursor::KeepAnchor);
}

void replaceSelection(QTextCursor &cursor, const QString &text) {
  cursor.insertText(text);
}

void toggleTag(QTextCursor &cursor, const QString &tag) {
  const TagPair tags = tagPair(tag);
  const QString selected = cursor.selectedText();

  if (selected == stripPair(selected, tags)) {
    replaceSelection(cursor, tags.open + selected + tags.close);
    return;
  }

  replaceSelection(cursor, stripPair(selected, tags));
}

QString stripHeading(const QString &line) {
  for (int level = 1; level <= 6; ++level) {
    const TagPair tags = tagPair(QStringLiteral("h%1").arg(level));

    const QString unwrapped = stripPair(line, tags);

    if (unwrapped != line) {
      return unwrapped;
    }
  }

  return stripPair(line, {"<p>", "</p>"});
}

QString stripListItem(const QString &line, const QString &listTag) {
  const QString prefix = QStringLiteral("<%1><li>").arg(listTag);

  const QString suffix = QStringLiteral("</li></%1>").arg(listTag);

  if (!line.startsWith(prefix) || !line.endsWith(suffix) ||
      line.size() < prefix.size() + suffix.size()) {
    return line;
  }

  return line.mid(prefix.size(), line.size() - prefix.size() - suffix.size());
}

bool hasTagPair(const QString &text, const QString &open,
                const QString &close) {
  return text.startsWith(open) && text.endsWith(close) &&
         text.size() >= open.size() + close.size();
}

bool isLineWrapped(const QString &line, const QString &open) {
  return line.startsWith(open);
}

} // namespace

void HTMLFormatDelegate::wrapTag(QTextCursor &cursor, const QString &tag) {
  toggleTag(cursor, tag);
}

// Inline formats

void HTMLFormatDelegate::toggleBold(QTextCursor &cursor) {
  toggleTag(cursor, QStringLiteral("strong"));
}

void HTMLFormatDelegate::toggleItalic(QTextCursor &cursor) {
  toggleTag(cursor, QStringLiteral("em"));
}

void HTMLFormatDelegate::toggleStrikethrough(QTextCursor &cursor) {
  toggleTag(cursor, QStringLiteral("s"));
}

void HTMLFormatDelegate::toggleCodeSpan(QTextCursor &cursor) {
  toggleTag(cursor, QStringLiteral("code"));
}

void HTMLFormatDelegate::toggleHighlight(QTextCursor &cursor) {
  toggleTag(cursor, QStringLiteral("mark"));
}

// Links & media

void HTMLFormatDelegate::insertLink(QTextCursor &cursor) {
  const QString selected = cursor.selectedText();

  static const QRegularExpression linkPattern(
      QStringLiteral(R"(^<a\s+href="[^"]*">(.*)</a>$)"));

  const QRegularExpressionMatch match = linkPattern.match(selected);

  if (match.hasMatch()) {
    replaceSelection(cursor, match.captured(1));
    return;
  }

  replaceSelection(cursor,
                   QStringLiteral("<a href=\"url\">%1</a>").arg(selected));
}

void HTMLFormatDelegate::insertWikiLink(QTextCursor &cursor) {
  const QString selected = cursor.selectedText();

  replaceSelection(cursor,
                   QStringLiteral("<a href=\"%1\">%1</a>").arg(selected));
}

void HTMLFormatDelegate::insertAutolink(QTextCursor &cursor) {
  insertLink(cursor);
}

void HTMLFormatDelegate::insertImage(QTextCursor &cursor) {
  const QString selected = cursor.selectedText();

  replaceSelection(
      cursor, QStringLiteral("<img src=\"url\" alt=\"%1\" />").arg(selected));
}

void HTMLFormatDelegate::insertMedia(QTextCursor &cursor) {
  const QString selected = cursor.selectedText();

  replaceSelection(
      cursor, QStringLiteral("<video src=\"url\">%1</video>").arg(selected));
}

// Blocks

void HTMLFormatDelegate::setHeadingLevel(QTextCursor &cursor, int level) {
  selectLine(cursor);

  const QString line = cursor.selectedText();
  const QString content = stripHeading(line);

  if (level >= 1 && level <= 6) {
    replaceSelection(cursor,
                     QStringLiteral("<h%1>%2</h%1>").arg(level).arg(content));

    return;
  }

  replaceSelection(cursor, QStringLiteral("<p>%1</p>").arg(content));
}

void HTMLFormatDelegate::toggleBlockQuote(QTextCursor &cursor) {
  selectLine(cursor);

  const QString line = cursor.selectedText();
  const TagPair tags{QStringLiteral("<blockquote>"),
                     QStringLiteral("</blockquote>")};

  replaceSelection(cursor, hasTagPair(line, tags.open, tags.close)
                               ? stripPair(line, tags)
                               : tags.open + line + tags.close);
}

void HTMLFormatDelegate::insertCallout(QTextCursor &cursor,
                                       const QString &type) {
  selectLine(cursor);

  const QString line = cursor.selectedText();
  const QString normalizedType = type.trimmed().toLower();

  replaceSelection(
      cursor,
      QStringLiteral("<div class=\"callout callout-%1\"><p>%2</p></div>")
          .arg(normalizedType)
          .arg(line));
}

void HTMLFormatDelegate::toggleBulletList(QTextCursor &cursor) {
  selectLine(cursor);

  const QString line = cursor.selectedText();
  const QString content = stripListItem(line, QStringLiteral("ul"));

  if (content != line) {
    replaceSelection(cursor, content);
    return;
  }

  replaceSelection(cursor, QStringLiteral("<ul><li>%1</li></ul>").arg(line));
}

void HTMLFormatDelegate::toggleOrderedList(QTextCursor &cursor) {
  selectLine(cursor);

  const QString line = cursor.selectedText();
  const QString content = stripListItem(line, QStringLiteral("ol"));

  if (content != line) {
    replaceSelection(cursor, content);
    return;
  }

  replaceSelection(cursor, QStringLiteral("<ol><li>%1</li></ol>").arg(line));
}

void HTMLFormatDelegate::toggleTaskItem(QTextCursor &cursor) {
  selectLine(cursor);

  const QString line = cursor.selectedText();

  static const QRegularExpression checkbox(
      QStringLiteral(R"(<input\s+type="checkbox"[^>]*\/?>)"));
  if (line.contains(checkbox)) {
    QString cleanedLine = line;
    cleanedLine.remove(checkbox);

    replaceSelection(cursor, cleanedLine);
    return;
  }

  replaceSelection(
      cursor, QStringLiteral("<label><input type=\"checkbox\" /> %1</label>")
                  .arg(line));
}

void HTMLFormatDelegate::insertDefinitionList(QTextCursor &cursor) {
  const QString selected = cursor.selectedText();
  const QString term = selected.isEmpty() ? QStringLiteral("Term") : selected;

  replaceSelection(
      cursor,
      QStringLiteral("<dl><dt>%1</dt><dd>Definition</dd></dl>").arg(term));
}

void HTMLFormatDelegate::toggleCodeBlock(QTextCursor &cursor) {
  const QString open = QStringLiteral("<pre><code>\n");
  const QString close = QStringLiteral("\n</code></pre>");
  const QString selected = cursor.selectedText();

  if (selected.startsWith(open) && selected.endsWith(close) &&
      selected.size() >= open.size() + close.size()) {
    replaceSelection(cursor,
                     selected.mid(open.size(), selected.size() - open.size() -
                                                   close.size()));

    return;
  }

  replaceSelection(cursor, open + selected + close);
}

void HTMLFormatDelegate::insertDiagramBlock(QTextCursor &cursor,
                                            const QString &engine) {
  const QString selected = cursor.selectedText();

  replaceSelection(cursor, QStringLiteral("<div class=\"%1\">%2</div>")
                               .arg(engine.trimmed().toLower())
                               .arg(selected));
}

void HTMLFormatDelegate::toggleMathBlock(QTextCursor &cursor) {
  toggleTag(cursor, QStringLiteral("math"));
}

void HTMLFormatDelegate::insertCollapsibleBlock(QTextCursor &cursor) {
  const QString selected = cursor.selectedText();

  replaceSelection(
      cursor, QStringLiteral("<details><summary>Title</summary>%1</details>")
                  .arg(selected));
}

void HTMLFormatDelegate::insertRawHtml(QTextCursor &cursor) {
  toggleTag(cursor, QStringLiteral("div"));
}

void HTMLFormatDelegate::insertHorizontalRule(QTextCursor &cursor) {
  replaceSelection(cursor, QStringLiteral("<hr />"));
}

void HTMLFormatDelegate::insertHardLineBreak(QTextCursor &cursor) {
  replaceSelection(cursor, QStringLiteral("<br />\n"));
}

// Indentation

void HTMLFormatDelegate::increaseIndent(QTextCursor &cursor) {
  toggleBlockQuote(cursor);
}

void HTMLFormatDelegate::decreaseIndent(QTextCursor &cursor) {
  selectLine(cursor);

  const QString line = cursor.selectedText();

  const TagPair tags{QStringLiteral("<blockquote>"),
                     QStringLiteral("</blockquote>")};

  if (!hasTagPair(line, tags.open, tags.close)) {
    return;
  }

  replaceSelection(cursor, stripPair(line, tags));
}

// Tables

void HTMLFormatDelegate::insertTable(QTextCursor &cursor) {
  replaceSelection(
      cursor, QStringLiteral("<table>\n"
                             "  <thead>\n"
                             "    <tr><th>Header 1</th><th>Header 2</th></tr>\n"
                             "  </thead>\n"
                             "  <tbody>\n"
                             "    <tr><td>Cell 1</td><td>Cell 2</td></tr>\n"
                             "  </tbody>\n"
                             "</table>"));
}

void HTMLFormatDelegate::deleteTable(QTextCursor &) {}

void HTMLFormatDelegate::addTableRow(QTextCursor &) {}

void HTMLFormatDelegate::removeTableRow(QTextCursor &) {}

void HTMLFormatDelegate::addTableColumn(QTextCursor &) {}

void HTMLFormatDelegate::removeTableColumn(QTextCursor &) {}

void HTMLFormatDelegate::alignTableColumnLeft(QTextCursor &) {}

void HTMLFormatDelegate::alignTableColumnCenter(QTextCursor &) {}

void HTMLFormatDelegate::alignTableColumnRight(QTextCursor &) {}

// Structure & meta

void HTMLFormatDelegate::insertFootnote(QTextCursor &cursor) {
  const QString selected = cursor.selectedText();
  const QString value = selected.isEmpty() ? QStringLiteral("1") : selected;

  replaceSelection(
      cursor, QStringLiteral("<sup><a href=\"#fn1\" id=\"ref1\">%1</a></sup>")
                  .arg(value));
}

void HTMLFormatDelegate::insertTag(QTextCursor &cursor) {
  toggleTag(cursor, QStringLiteral("span class=\"tag\""));
}

void HTMLFormatDelegate::insertTableOfContents(QTextCursor &cursor) {
  replaceSelection(cursor, QStringLiteral("<nav class=\"toc\"></nav>"));
}

void HTMLFormatDelegate::toggleFrontmatter(QTextCursor &cursor) {
  replaceSelection(cursor, QStringLiteral("<!--\n"
                                          "title: \n"
                                          "date: \n"
                                          "-->\n"));
}

// State queries

bool HTMLFormatDelegate::isBold(const QTextCursor &cursor) const {
  const QString selected = cursor.selectedText();

  return hasTagPair(selected, QStringLiteral("<strong>"),
                    QStringLiteral("</strong>")) ||
         hasTagPair(selected, QStringLiteral("<b>"), QStringLiteral("</b>"));
}

bool HTMLFormatDelegate::isItalic(const QTextCursor &cursor) const {
  const QString selected = cursor.selectedText();

  return hasTagPair(selected, QStringLiteral("<em>"),
                    QStringLiteral("</em>")) ||
         hasTagPair(selected, QStringLiteral("<i>"), QStringLiteral("</i>"));
}

bool HTMLFormatDelegate::isStrikethrough(const QTextCursor &cursor) const {
  const QString selected = cursor.selectedText();

  return hasTagPair(selected, QStringLiteral("<s>"), QStringLiteral("</s>")) ||
         hasTagPair(selected, QStringLiteral("<del>"),
                    QStringLiteral("</del>"));
}

bool HTMLFormatDelegate::isCodeSpan(const QTextCursor &cursor) const {
  return hasTagPair(cursor.selectedText(), QStringLiteral("<code>"),
                    QStringLiteral("</code>"));
}

bool HTMLFormatDelegate::isHighlight(const QTextCursor &cursor) const {
  return hasTagPair(cursor.selectedText(), QStringLiteral("<mark>"),
                    QStringLiteral("</mark>"));
}

bool HTMLFormatDelegate::isBlockQuote(const QTextCursor &cursor) const {
  return isLineWrapped(cursor.block().text().trimmed(),
                       QStringLiteral("<blockquote>"));
}

bool HTMLFormatDelegate::isBulletList(const QTextCursor &cursor) const {
  return isLineWrapped(cursor.block().text().trimmed(), QStringLiteral("<ul>"));
}

bool HTMLFormatDelegate::isOrderedList(const QTextCursor &cursor) const {
  return isLineWrapped(cursor.block().text().trimmed(), QStringLiteral("<ol>"));
}

bool HTMLFormatDelegate::isTaskList(const QTextCursor &cursor) const {
  return cursor.block().text().contains(QStringLiteral("type=\"checkbox\""));
}

bool HTMLFormatDelegate::isCodeBlock(const QTextCursor &cursor) const {
  return isLineWrapped(cursor.block().text().trimmed(),
                       QStringLiteral("<pre>"));
}

bool HTMLFormatDelegate::isMathBlock(const QTextCursor &cursor) const {
  return isLineWrapped(cursor.block().text().trimmed(),
                       QStringLiteral("<math>"));
}

int HTMLFormatDelegate::currentHeadingLevel(const QTextCursor &cursor) const {
  const QString line = cursor.block().text().trimmed();

  for (int level = 1; level <= 6; ++level) {
    if (line.startsWith(QStringLiteral("<h%1>").arg(level))) {
      return level;
    }
  }

  return 0;
}

bool HTMLFormatDelegate::isInsideTable(const QTextCursor &cursor) const {
  const QString text = cursor.block().text();

  return text.contains(QStringLiteral("<td")) ||
         text.contains(QStringLiteral("<th")) ||
         text.contains(QStringLiteral("<tr"));
}