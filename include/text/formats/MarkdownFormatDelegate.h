#ifndef EPISTEME_MARKDOWNFORMATDELEGATE_H
#define EPISTEME_MARKDOWNFORMATDELEGATE_H

#include "TextFormatDelegate.h"

class MarkdownFormatDelegate : public TextFormatDelegate {
public:
  void toggleBold(QTextCursor &cursor) override;
  void toggleItalic(QTextCursor &cursor) override;
  void toggleStrikethrough(QTextCursor &cursor) override;
  void toggleCodeSpan(QTextCursor &cursor) override;
  void toggleHighlight(QTextCursor &cursor) override;

  void insertLink(QTextCursor &cursor) override;
  void insertWikiLink(QTextCursor &cursor) override;
  void insertAutolink(QTextCursor &cursor) override;
  void insertImage(QTextCursor &cursor) override;
  void insertMedia(QTextCursor &cursor) override;

  void setHeadingLevel(QTextCursor &cursor, int level) override;
  void toggleBlockQuote(QTextCursor &cursor) override;
  void insertCallout(QTextCursor &cursor, const QString &type) override;
  void toggleBulletList(QTextCursor &cursor) override;
  void toggleOrderedList(QTextCursor &cursor) override;
  void toggleTaskItem(QTextCursor &cursor) override;
  void insertDefinitionList(QTextCursor &cursor) override;
  void toggleCodeBlock(QTextCursor &cursor) override;
  void insertDiagramBlock(QTextCursor &cursor, const QString &engine) override;
  void toggleMathBlock(QTextCursor &cursor) override;
  void insertCollapsibleBlock(QTextCursor &cursor) override;
  void insertRawHtml(QTextCursor &cursor) override;
  void insertHorizontalRule(QTextCursor &cursor) override;
  void insertHardLineBreak(QTextCursor &cursor) override;

  void increaseIndent(QTextCursor &cursor) override;
  void decreaseIndent(QTextCursor &cursor) override;

  void insertTable(QTextCursor &cursor) override;
  void deleteTable(QTextCursor &cursor) override;
  void addTableRow(QTextCursor &cursor) override;
  void removeTableRow(QTextCursor &cursor) override;
  void addTableColumn(QTextCursor &cursor) override;
  void removeTableColumn(QTextCursor &cursor) override;
  void alignTableColumnLeft(QTextCursor &cursor) override;
  void alignTableColumnCenter(QTextCursor &cursor) override;
  void alignTableColumnRight(QTextCursor &cursor) override;

  void insertFootnote(QTextCursor &cursor) override;
  void insertTag(QTextCursor &cursor) override;
  void insertTableOfContents(QTextCursor &cursor) override;
  void toggleFrontmatter(QTextCursor &cursor) override;

  bool isBold(const QTextCursor &cursor) const override;
  bool isItalic(const QTextCursor &cursor) const override;
  bool isStrikethrough(const QTextCursor &cursor) const override;
  bool isCodeSpan(const QTextCursor &cursor) const override;
  bool isHighlight(const QTextCursor &cursor) const override;
  bool isBlockQuote(const QTextCursor &cursor) const override;
  bool isBulletList(const QTextCursor &cursor) const override;
  bool isOrderedList(const QTextCursor &cursor) const override;
  bool isTaskList(const QTextCursor &cursor) const override;
  bool isCodeBlock(const QTextCursor &cursor) const override;
  bool isMathBlock(const QTextCursor &cursor) const override;
  int currentHeadingLevel(const QTextCursor &cursor) const override;
  bool isInsideTable(const QTextCursor &cursor) const override;

private:
  void wrapSelection(QTextCursor &cursor, const QString &prefix,
                     const QString &suffix);
  void toggleLinePrefix(QTextCursor &cursor, const QString &prefix);
};

#endif // EPISTEME_MARKDOWNFORMATDELEGATE_H