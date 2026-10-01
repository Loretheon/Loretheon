#ifndef TOOLBAR_H
#define TOOLBAR_H

#include <QAction>
#include <QComboBox>
#include <QToolBar>

class TextEdit;
class Toolbar : public QToolBar {
  Q_OBJECT

public:
  explicit Toolbar(QWidget *parent = nullptr);
  ~Toolbar() override = default;

  void setTextEdit(TextEdit *editor);

private slots:
  void updateActionsState();
  void onHeadingLevelChanged(int index);
  void onInsertCallout();
  void onInsertDiagram();

private:
  void setupToolbarStyle();
  QIcon createThemedIcon(const QString &symbolName);

  TextEdit *m_editor = nullptr;

  QAction *m_boldAction = nullptr;
  QAction *m_italicAction = nullptr;
  QAction *m_strikethroughAction = nullptr;
  QAction *m_codeSpanAction = nullptr;
  QAction *m_highlightAction = nullptr;

  QAction *m_linkAction = nullptr;
  QAction *m_wikiLinkAction = nullptr;
  QAction *m_autolinkAction = nullptr;
  QAction *m_imageAction = nullptr;
  QAction *m_mediaAction = nullptr;

  QComboBox *m_headingCombo = nullptr;

  QAction *m_blockquoteAction = nullptr;
  QAction *m_calloutAction = nullptr;
  QAction *m_bulletListAction = nullptr;
  QAction *m_orderedListAction = nullptr;
  QAction *m_taskListAction = nullptr;
  QAction *m_definitionListAction = nullptr;
  QAction *m_codeBlockAction = nullptr;
  QAction *m_diagramBlockAction = nullptr;
  QAction *m_mathBlockAction = nullptr;
  QAction *m_detailsAction = nullptr;
  QAction *m_rawHtmlAction = nullptr;
  QAction *m_hrAction = nullptr;
  QAction *m_hardBreakAction = nullptr;

  QAction *m_increaseIndentAction = nullptr;
  QAction *m_decreaseIndentAction = nullptr;

  QAction *m_insertTableAction = nullptr;
  QAction *m_deleteTableAction = nullptr;
  QAction *m_addRowAction = nullptr;
  QAction *m_removeRowAction = nullptr;
  QAction *m_addColumnAction = nullptr;
  QAction *m_removeColumnAction = nullptr;
  QAction *m_alignTableLeftAction = nullptr;
  QAction *m_alignTableCenterAction = nullptr;
  QAction *m_alignTableRightAction = nullptr;

  QAction *m_footnoteAction = nullptr;
  QAction *m_tagAction = nullptr;
  QAction *m_tocAction = nullptr;
  QAction *m_frontmatterAction = nullptr;

  QAction *m_toggleSourceViewAction = nullptr;
};

#endif // TOOLBAR_H