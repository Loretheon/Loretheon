#ifndef EPISTEME_TEXTEDIT_H
#define EPISTEME_TEXTEDIT_H

#include "DocumentMode.h"

#include "TextFormatDelegate.h"
#include "Toolbar.h"
#include <QPlainTextEdit>
#include <memory>

class TextEdit : public QPlainTextEdit {
  Q_OBJECT

public:
  Q_ENUM(DocumentMode)

  explicit TextEdit(QWidget *parent = nullptr);

  Toolbar *toolbar() const { return m_toolbar; }
  DocumentMode documentMode() const { return m_mode; }
  bool openFile(const QString &filePath);

  bool isBold() const;
  bool isItalic() const;
  bool isStrikethrough() const;
  bool isCodeSpan() const;
  bool isHighlight() const;
  bool isBlockQuote() const;
  bool isBulletList() const;
  bool isOrderedList() const;
  bool isTaskList() const;
  bool isCodeBlock() const;
  bool isMathBlock() const;
  int currentHeadingLevel() const;
  bool isInsideTable() const;
  bool isSourceMode() const { return m_sourceMode; }

public slots:
  void setDocumentMode(DocumentMode mode);

  void toggleBold();
  void toggleItalic();
  void toggleStrikethrough();
  void toggleCodeSpan();
  void toggleHighlight();

  void insertLink();
  void insertWikiLink();
  void insertAutolink();
  void insertImage();
  void insertMedia();

  void setHeadingLevel(int level);
  void toggleBlockQuote();
  void insertCallout(const QString &type);
  void toggleBulletList();
  void toggleOrderedList();
  void toggleTaskItem();
  void insertDefinitionList();
  void toggleCodeBlock();
  void insertDiagramBlock(const QString &engine);
  void toggleMathBlock();
  void insertCollapsibleBlock();
  void insertRawHtml();
  void insertHorizontalRule();
  void insertHardLineBreak();

  void increaseIndent();
  void decreaseIndent();

  void insertTable();
  void deleteTable();
  void addTableRow();
  void removeTableRow();
  void addTableColumn();
  void removeTableColumn();
  void alignTableColumnLeft();
  void alignTableColumnCenter();
  void alignTableColumnRight();

  void insertFootnote();
  void insertTag();
  void insertTableOfContents();
  void toggleFrontmatter();

  void toggleSourceMode();

signals:
  void formatChanged();

private:
  void setupToolbar();
  void setupToolbarConnections();

  Toolbar *m_toolbar = nullptr;
  DocumentMode m_mode{DocumentMode::PlainText};
  std::unique_ptr<TextFormatDelegate> m_delegate;
  bool m_sourceMode{false};
};

#endif // EPISTEME_TEXTEDIT_H