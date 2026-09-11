#include "../../include/text/Toolbar.h"

#include "TextEdit.h"

#include <QInputDialog>
#include <QMenu>
#include <QSignalBlocker>

namespace {

struct ActionBinding {
  QAction *action;
  void (TextEdit::*handler)();
};

constexpr const char *Icons = "icons";

} // namespace

Toolbar::Toolbar(QWidget *parent) : QToolBar(parent) {
  setToolButtonStyle(Qt::ToolButtonIconOnly);
  setIconSize(QSize(20, 20));

  // Text formatting
  m_boldAction = addAction(QIcon::fromTheme(QStringLiteral("format-text-bold")),
                           tr("Bold"));
  m_boldAction->setCheckable(true);

  m_italicAction = addAction(
      QIcon::fromTheme(QStringLiteral("format-text-italic")), tr("Italic"));
  m_italicAction->setCheckable(true);

  m_strikethroughAction =
      addAction(QIcon::fromTheme(QStringLiteral("format-text-strikethrough")),
                tr("Strikethrough"));
  m_strikethroughAction->setCheckable(true);

  m_codeSpanAction = addAction(
      QIcon::fromTheme(QStringLiteral("format-text-code")), tr("Inline Code"));
  m_codeSpanAction->setCheckable(true);

  m_highlightAction = addAction(
      QIcon::fromTheme(QStringLiteral("format-highlight")), tr("Highlight"));
  m_highlightAction->setCheckable(true);

  addSeparator();

  // Links & media
  m_linkAction = addAction(QIcon::fromTheme(QStringLiteral("insert-link")),
                           tr("Insert Link"));

  m_wikiLinkAction = addAction(QIcon::fromTheme(QStringLiteral("insert-link")),
                               tr("Internal Link"));

  m_autolinkAction =
      addAction(QIcon::fromTheme(QStringLiteral("link")), tr("Autolink"));

  m_imageAction = addAction(QIcon::fromTheme(QStringLiteral("insert-image")),
                            tr("Insert Image"));

  m_mediaAction = addAction(QIcon::fromTheme(QStringLiteral("video-x-generic")),
                            tr("Insert Media"));

  addSeparator();

  // Paragraph / heading
  m_headingCombo = new QComboBox(this);

  m_headingCombo->addItem(tr("Paragraph"), 0);

  for (int level = 1; level <= 6; ++level) {
    m_headingCombo->addItem(tr("Heading %1").arg(level), level);
  }

  m_headingCombo->setToolTip(tr("Paragraph / Heading"));

  addWidget(m_headingCombo);

  m_blockquoteAction =
      addAction(QIcon::fromTheme(QStringLiteral("format-quote")), tr("Quote"));
  m_blockquoteAction->setCheckable(true);

  m_calloutAction = addAction(
      QIcon::fromTheme(QStringLiteral("dialog-information")), tr("Callout"));

  addSeparator();

  // Lists
  m_bulletListAction =
      addAction(QIcon::fromTheme(QStringLiteral("format-list-unordered")),
                tr("Bullet List"));
  m_bulletListAction->setCheckable(true);

  m_orderedListAction =
      addAction(QIcon::fromTheme(QStringLiteral("format-list-ordered")),
                tr("Numbered List"));
  m_orderedListAction->setCheckable(true);

  m_taskListAction =
      addAction(QIcon::fromTheme(QStringLiteral("checkbox")), tr("Task List"));
  m_taskListAction->setCheckable(true);

  m_definitionListAction =
      addAction(QIcon::fromTheme(QStringLiteral("format-list-unordered")),
                tr("Definition List"));

  addSeparator();

  // Blocks
  m_codeBlockAction = addAction(
      QIcon::fromTheme(QStringLiteral("text-x-generic")), tr("Code Block"));
  m_codeBlockAction->setCheckable(true);

  m_diagramBlockAction = addAction(
      QIcon::fromTheme(QStringLiteral("view-refresh")), tr("Diagram"));

  m_mathBlockAction =
      addAction(QIcon::fromTheme(QStringLiteral("accessories-calculator")),
                tr("Math Block"));
  m_mathBlockAction->setCheckable(true);

  m_detailsAction =
      addAction(QIcon::fromTheme(QStringLiteral("view-list-details")),
                tr("Collapsible Block"));

  m_rawHtmlAction = addAction(QIcon::fromTheme(QStringLiteral("text-html")),
                              tr("HTML Block"));

  m_hrAction =
      addAction(QIcon::fromTheme(QStringLiteral("format-justify-fill")),
                tr("Horizontal Rule"));

  m_hardBreakAction =
      addAction(QIcon::fromTheme(QStringLiteral("go-next")), tr("Hard Break"));

  addSeparator();

  // Indentation
  m_decreaseIndentAction = addAction(
      QIcon::fromTheme(QStringLiteral("format-indent-less")), tr("Outdent"));

  m_increaseIndentAction = addAction(
      QIcon::fromTheme(QStringLiteral("format-indent-more")), tr("Indent"));

  addSeparator();

  // Tables
  m_insertTableAction = addAction(
      QIcon::fromTheme(QStringLiteral("insert-table")), tr("Insert Table"));

  m_deleteTableAction = addAction(
      QIcon::fromTheme(QStringLiteral("edit-delete")), tr("Delete Table"));

  m_addRowAction =
      addAction(QIcon::fromTheme(QStringLiteral("list-add")), tr("Add Row"));

  m_removeRowAction = addAction(QIcon::fromTheme(QStringLiteral("list-remove")),
                                tr("Remove Row"));

  m_addColumnAction =
      addAction(QIcon::fromTheme(QStringLiteral("list-add")), tr("Add Column"));

  m_removeColumnAction = addAction(
      QIcon::fromTheme(QStringLiteral("list-remove")), tr("Remove Column"));

  m_alignTableLeftAction =
      addAction(QIcon::fromTheme(QStringLiteral("format-justify-left")),
                tr("Align Left"));

  m_alignTableCenterAction =
      addAction(QIcon::fromTheme(QStringLiteral("format-justify-center")),
                tr("Align Center"));

  m_alignTableRightAction =
      addAction(QIcon::fromTheme(QStringLiteral("format-justify-right")),
                tr("Align Right"));

  addSeparator();

  // Document
  m_footnoteAction =
      addAction(QIcon::fromTheme(QStringLiteral("footnote")), tr("Footnote"));

  m_tagAction =
      addAction(QIcon::fromTheme(QStringLiteral("tag")), tr("Insert Tag"));

  m_tocAction = addAction(QIcon::fromTheme(QStringLiteral("view-list-tree")),
                          tr("Table of Contents"));

  m_frontmatterAction =
      addAction(QIcon::fromTheme(QStringLiteral("document-properties")),
                tr("Front Matter"));

  addSeparator();

  // View
  m_toggleSourceViewAction =
      addAction(QIcon::fromTheme(QStringLiteral("text-x-generic")),
                tr("Toggle Source / Preview"));

  m_toggleSourceViewAction->setCheckable(true);

  connect(m_headingCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &Toolbar::onHeadingLevelChanged);

  connect(m_calloutAction, &QAction::triggered, this,
          &Toolbar::onInsertCallout);

  connect(m_diagramBlockAction, &QAction::triggered, this,
          &Toolbar::onInsertDiagram);
}

void Toolbar::setTextEdit(TextEdit *editor) {
  if (m_editor == editor) {
    updateActionsState();
    return;
  }

  if (m_editor) {
    disconnect(m_editor, nullptr, this, nullptr);
  }

  m_editor = editor;

  if (!m_editor) {
    updateActionsState();
    return;
  }

  const ActionBinding bindings[] = {
      {m_boldAction, &TextEdit::toggleBold},
      {m_italicAction, &TextEdit::toggleItalic},
      {m_strikethroughAction, &TextEdit::toggleStrikethrough},
      {m_codeSpanAction, &TextEdit::toggleCodeSpan},
      {m_highlightAction, &TextEdit::toggleHighlight},

      {m_linkAction, &TextEdit::insertLink},
      {m_wikiLinkAction, &TextEdit::insertWikiLink},
      {m_autolinkAction, &TextEdit::insertAutolink},
      {m_imageAction, &TextEdit::insertImage},
      {m_mediaAction, &TextEdit::insertMedia},

      {m_blockquoteAction, &TextEdit::toggleBlockQuote},
      {m_bulletListAction, &TextEdit::toggleBulletList},
      {m_orderedListAction, &TextEdit::toggleOrderedList},
      {m_taskListAction, &TextEdit::toggleTaskItem},
      {m_definitionListAction, &TextEdit::insertDefinitionList},

      {m_codeBlockAction, &TextEdit::toggleCodeBlock},
      {m_mathBlockAction, &TextEdit::toggleMathBlock},
      {m_detailsAction, &TextEdit::insertCollapsibleBlock},
      {m_rawHtmlAction, &TextEdit::insertRawHtml},
      {m_hrAction, &TextEdit::insertHorizontalRule},
      {m_hardBreakAction, &TextEdit::insertHardLineBreak},

      {m_increaseIndentAction, &TextEdit::increaseIndent},
      {m_decreaseIndentAction, &TextEdit::decreaseIndent},

      {m_insertTableAction, &TextEdit::insertTable},
      {m_deleteTableAction, &TextEdit::deleteTable},
      {m_addRowAction, &TextEdit::addTableRow},
      {m_removeRowAction, &TextEdit::removeTableRow},
      {m_addColumnAction, &TextEdit::addTableColumn},
      {m_removeColumnAction, &TextEdit::removeTableColumn},
      {m_alignTableLeftAction, &TextEdit::alignTableColumnLeft},
      {m_alignTableCenterAction, &TextEdit::alignTableColumnCenter},
      {m_alignTableRightAction, &TextEdit::alignTableColumnRight},

      {m_footnoteAction, &TextEdit::insertFootnote},
      {m_tagAction, &TextEdit::insertTag},
      {m_tocAction, &TextEdit::insertTableOfContents},
      {m_frontmatterAction, &TextEdit::toggleFrontmatter},

      {m_toggleSourceViewAction, &TextEdit::toggleSourceMode},
  };

  for (const ActionBinding &binding : bindings) {
    connect(binding.action, &QAction::triggered, m_editor, binding.handler);
  }

  connect(m_editor, &TextEdit::formatChanged, this,
          &Toolbar::updateActionsState);

  updateActionsState();
}

void Toolbar::updateActionsState() {
  if (!m_editor) {
    QAction *checkableActions[] = {
        m_boldAction,       m_italicAction,      m_strikethroughAction,
        m_codeSpanAction,   m_highlightAction,   m_blockquoteAction,
        m_bulletListAction, m_orderedListAction, m_taskListAction,
        m_codeBlockAction,  m_mathBlockAction,   m_toggleSourceViewAction};

    for (QAction *action : checkableActions) {
      action->setChecked(false);
    }

    {
      QSignalBlocker blocker(m_headingCombo);

      const int paragraphIndex = m_headingCombo->findData(0);

      if (paragraphIndex >= 0) {
        m_headingCombo->setCurrentIndex(paragraphIndex);
      }
    }

    QAction *tableActions[] = {
        m_deleteTableAction,      m_addRowAction,
        m_removeRowAction,        m_addColumnAction,
        m_removeColumnAction,     m_alignTableLeftAction,
        m_alignTableCenterAction, m_alignTableRightAction};

    for (QAction *action : tableActions) {
      action->setEnabled(false);
    }

    return;
  }

  m_boldAction->setChecked(m_editor->isBold());
  m_italicAction->setChecked(m_editor->isItalic());
  m_strikethroughAction->setChecked(m_editor->isStrikethrough());
  m_codeSpanAction->setChecked(m_editor->isCodeSpan());
  m_highlightAction->setChecked(m_editor->isHighlight());

  m_blockquoteAction->setChecked(m_editor->isBlockQuote());
  m_bulletListAction->setChecked(m_editor->isBulletList());
  m_orderedListAction->setChecked(m_editor->isOrderedList());
  m_taskListAction->setChecked(m_editor->isTaskList());
  m_codeBlockAction->setChecked(m_editor->isCodeBlock());
  m_mathBlockAction->setChecked(m_editor->isMathBlock());

  {
    QSignalBlocker blocker(m_headingCombo);

    const int index = m_headingCombo->findData(m_editor->currentHeadingLevel());

    if (index >= 0) {
      m_headingCombo->setCurrentIndex(index);
    }
  }

  const bool inTable = m_editor->isInsideTable();

  QAction *tableActions[] = {m_deleteTableAction,      m_addRowAction,
                             m_removeRowAction,        m_addColumnAction,
                             m_removeColumnAction,     m_alignTableLeftAction,
                             m_alignTableCenterAction, m_alignTableRightAction};

  for (QAction *action : tableActions) {
    action->setEnabled(inTable);
  }

  m_toggleSourceViewAction->setChecked(m_editor->isSourceMode());
}
void Toolbar::onHeadingLevelChanged(int index) {
  if (!m_editor || index < 0) {
    return;
  }

  const int level = m_headingCombo->itemData(index).toInt();

  m_editor->setHeadingLevel(level);
}

void Toolbar::onInsertCallout() {
  if (!m_editor) {
    return;
  }

  const QStringList types = {tr("NOTE"), tr("TIP"), tr("WARNING"),
                             tr("CAUTION"), tr("IMPORTANT")};

  bool accepted = false;

  const QString type = QInputDialog::getItem(
      this, tr("Insert Callout"), tr("Type:"), types, 0, false, &accepted);

  if (accepted && !type.isEmpty()) {
    m_editor->insertCallout(type);
  }
}

void Toolbar::onInsertDiagram() {
  if (!m_editor) {
    return;
  }

  const QStringList engines = {QStringLiteral("mermaid"),
                               QStringLiteral("plantuml")};

  bool accepted = false;

  const QString engine = QInputDialog::getItem(
      this, tr("Insert Diagram"), tr("Engine:"), engines, 0, false, &accepted);

  if (accepted && !engine.isEmpty()) {
    m_editor->insertDiagramBlock(engine);
  }
}