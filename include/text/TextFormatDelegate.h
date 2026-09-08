#ifndef EPISTEME_TEXTFORMATDELEGATE_H
#define EPISTEME_TEXTFORMATDELEGATE_H

#include <QString>
#include <QTextCursor>

class TextFormatDelegate
{
public:
    virtual ~TextFormatDelegate() = default;

    // Inline
    virtual void toggleBold(QTextCursor &cursor) = 0;
    virtual void toggleItalic(QTextCursor &cursor) = 0;
    virtual void toggleStrikethrough(QTextCursor &cursor) = 0;
    virtual void toggleCodeSpan(QTextCursor &cursor) = 0;
    virtual void toggleHighlight(QTextCursor &cursor) = 0;

    // Links & Media
    virtual void insertLink(QTextCursor &cursor) = 0;
    virtual void insertWikiLink(QTextCursor &cursor) = 0;
    virtual void insertAutolink(QTextCursor &cursor) = 0;
    virtual void insertImage(QTextCursor &cursor) = 0;
    virtual void insertMedia(QTextCursor &cursor) = 0;

    // Blocks
    virtual void setHeadingLevel(QTextCursor &cursor, int level) = 0;
    virtual void toggleBlockQuote(QTextCursor &cursor) = 0;
    virtual void insertCallout(QTextCursor &cursor, const QString &type) = 0;
    virtual void toggleBulletList(QTextCursor &cursor) = 0;
    virtual void toggleOrderedList(QTextCursor &cursor) = 0;
    virtual void toggleTaskItem(QTextCursor &cursor) = 0;
    virtual void insertDefinitionList(QTextCursor &cursor) = 0;
    virtual void toggleCodeBlock(QTextCursor &cursor) = 0;
    virtual void insertDiagramBlock(QTextCursor &cursor, const QString &engine) = 0;
    virtual void toggleMathBlock(QTextCursor &cursor) = 0;
    virtual void insertCollapsibleBlock(QTextCursor &cursor) = 0;
    virtual void insertRawHtml(QTextCursor &cursor) = 0;
    virtual void insertHorizontalRule(QTextCursor &cursor) = 0;
    virtual void insertHardLineBreak(QTextCursor &cursor) = 0;

    // Indentation
    virtual void increaseIndent(QTextCursor &cursor) = 0;
    virtual void decreaseIndent(QTextCursor &cursor) = 0;

    // Tables
    virtual void insertTable(QTextCursor &cursor) = 0;
    virtual void deleteTable(QTextCursor &cursor) = 0;
    virtual void addTableRow(QTextCursor &cursor) = 0;
    virtual void removeTableRow(QTextCursor &cursor) = 0;
    virtual void addTableColumn(QTextCursor &cursor) = 0;
    virtual void removeTableColumn(QTextCursor &cursor) = 0;
    virtual void alignTableColumnLeft(QTextCursor &cursor) = 0;
    virtual void alignTableColumnCenter(QTextCursor &cursor) = 0;
    virtual void alignTableColumnRight(QTextCursor &cursor) = 0;

    // Structure & Meta
    virtual void insertFootnote(QTextCursor &cursor) = 0;
    virtual void insertTag(QTextCursor &cursor) = 0;
    virtual void insertTableOfContents(QTextCursor &cursor) = 0;
    virtual void toggleFrontmatter(QTextCursor &cursor) = 0;

    // State Queries
    virtual bool isBold(const QTextCursor &cursor) const = 0;
    virtual bool isItalic(const QTextCursor &cursor) const = 0;
    virtual bool isStrikethrough(const QTextCursor &cursor) const = 0;
    virtual bool isCodeSpan(const QTextCursor &cursor) const = 0;
    virtual bool isHighlight(const QTextCursor &cursor) const = 0;
    virtual bool isBlockQuote(const QTextCursor &cursor) const = 0;
    virtual bool isBulletList(const QTextCursor &cursor) const = 0;
    virtual bool isOrderedList(const QTextCursor &cursor) const = 0;
    virtual bool isTaskList(const QTextCursor &cursor) const = 0;
    virtual bool isCodeBlock(const QTextCursor &cursor) const = 0;
    virtual bool isMathBlock(const QTextCursor &cursor) const = 0;
    virtual int currentHeadingLevel(const QTextCursor &cursor) const = 0;
    virtual bool isInsideTable(const QTextCursor &cursor) const = 0;
};

#endif // EPISTEME_TEXTFORMATDELEGATE_H