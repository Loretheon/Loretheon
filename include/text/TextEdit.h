#ifndef EPISTEME_TEXTEDIT_H
#define EPISTEME_TEXTEDIT_H

#include "DocumentMode.h"
#include "TextFormatDelegate.h"
#include "Toolbar.h"
#include <QMouseEvent>
#include "../ai/edit/PendingEdit.h"

#include <QHash>
#include <QPlainTextEdit>
#include <QString>

#include <memory>

class QCheckBox;
class QLabel;
class QLayout;
class QPushButton;
class QResizeEvent;
class QScrollArea;
class QWidget;

class TextEdit : public QPlainTextEdit {
  Q_OBJECT

public:
  Q_ENUM(DocumentMode)

  explicit TextEdit(QWidget *parent = nullptr);

  Toolbar *toolbar() const { return m_toolbar; }

  DocumentMode documentMode() const { return m_mode; }

  bool openFile(const QString &filePath);

  static QString lastOpenedFile();

  bool autoAcceptEdits() const { return m_autoAcceptEdits; }

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

  void showPendingEdit(const PendingEdit &edit);

  void updatePendingEdit(const PendingEdit &edit);

  void removePendingEdit(int editId);

  void clearPendingEdits();

  void refreshPendingEdits();

public slots:
  void setDocumentMode(DocumentMode mode);

  void setAutoAcceptEdits(bool enabled);

  void setPendingEditAccepted(int editId, bool accepted);

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

  void acceptPendingEditRequested(int editId);

  void rejectPendingEditRequested(int editId);

  void acceptAllPendingEditsRequested();

  void rejectAllPendingEditsRequested();

  void autoAcceptChanged(bool enabled);

protected:
  void resizeEvent(QResizeEvent *event) override;
  void mouseMoveEvent(QMouseEvent *event) override;
private:
  QString pendingEditPreview(const PendingEdit &edit) const;

  const PendingEdit *pendingEditAtPosition(int position) const;
  void setupToolbar();

  void setupToolbarConnections();

  void setupReviewBar();

  void rebuildReviewRows();

  void updateReviewBar();

  void updatePendingHighlight();

  QString pendingEditSummary(const PendingEdit &edit) const;

  QString pendingEditStatus(const PendingEdit &edit) const;

  QWidget *createReviewRow(const PendingEdit &edit);

  void setReviewRowStyle(QWidget *row, const PendingEdit &edit);

  Toolbar *m_toolbar = nullptr;

  QWidget *m_reviewBar = nullptr;

  QScrollArea *m_reviewScrollArea = nullptr;

  QWidget *m_reviewContent = nullptr;

  QLayout *m_reviewLayout = nullptr;

  QLabel *m_reviewSummary = nullptr;

  QPushButton *m_acceptAllButton = nullptr;

  QPushButton *m_rejectAllButton = nullptr;

  QCheckBox *m_autoAcceptCheckBox = nullptr;

  DocumentMode m_mode{DocumentMode::PlainText};

  std::unique_ptr<TextFormatDelegate> m_delegate;

  bool m_sourceMode{false};

  bool m_autoAcceptEdits{false};

  QHash<int, PendingEdit> m_pendingEdits;
};

#endif // EPISTEME_TEXTEDIT_H