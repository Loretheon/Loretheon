#include "../../include/text/Toolbar.h"

#include "TextEdit.h"

#include <QInputDialog>
#include <QMenu>

Toolbar::Toolbar(QWidget *parent)
    : QToolBar(parent)
{
    // Show icons only; action text is still used for tooltips/accessibility.
    setToolButtonStyle(Qt::ToolButtonIconOnly);
    setIconSize(QSize(20, 20));

    // -------------------------------------------------------------------------
    // Text formatting
    // -------------------------------------------------------------------------

    m_boldAction = addAction(
        QIcon::fromTheme("format-text-bold"),
        "Bold"
    );
    m_boldAction->setCheckable(true);

    m_italicAction = addAction(
        QIcon::fromTheme("format-text-italic"),
        "Italic"
    );
    m_italicAction->setCheckable(true);

    m_strikethroughAction = addAction(
        QIcon::fromTheme("format-text-strikethrough"),
        "Strikethrough"
    );
    m_strikethroughAction->setCheckable(true);

    m_codeSpanAction = addAction(
        QIcon::fromTheme("format-text-code"),
        "Inline Code"
    );
    m_codeSpanAction->setCheckable(true);

    m_highlightAction = addAction(
        QIcon::fromTheme("format-highlight"),
        "Highlight"
    );
    m_highlightAction->setCheckable(true);

    addSeparator();

    // -------------------------------------------------------------------------
    // Links and media
    // -------------------------------------------------------------------------

    m_linkAction = addAction(
        QIcon::fromTheme("insert-link"),
        "Insert Link"
    );

    m_wikiLinkAction = addAction(
        QIcon::fromTheme("insert-link"),
        "Internal Link"
    );

    m_autolinkAction = addAction(
        QIcon::fromTheme("link"),
        "Autolink"
    );

    m_imageAction = addAction(
        QIcon::fromTheme("insert-image"),
        "Insert Image"
    );

    m_mediaAction = addAction(
        QIcon::fromTheme("video-x-generic"),
        "Insert Media"
    );

    addSeparator();

    // -------------------------------------------------------------------------
    // Paragraph / heading
    // -------------------------------------------------------------------------

    m_headingCombo = new QComboBox(this);
    m_headingCombo->addItem("Paragraph", 0);
    m_headingCombo->addItem("Heading 1", 1);
    m_headingCombo->addItem("Heading 2", 2);
    m_headingCombo->addItem("Heading 3", 3);
    m_headingCombo->addItem("Heading 4", 4);
    m_headingCombo->addItem("Heading 5", 5);
    m_headingCombo->addItem("Heading 6", 6);
    m_headingCombo->setToolTip("Paragraph / Heading");

    addWidget(m_headingCombo);

    m_blockquoteAction = addAction(
        QIcon::fromTheme("format-quote"),
        "Quote"
    );
    m_blockquoteAction->setCheckable(true);

    m_calloutAction = addAction(
        QIcon::fromTheme("dialog-information"),
        "Callout"
    );

    addSeparator();

    // -------------------------------------------------------------------------
    // Lists
    // -------------------------------------------------------------------------

    m_bulletListAction = addAction(
        QIcon::fromTheme("format-list-unordered"),
        "Bullet List"
    );
    m_bulletListAction->setCheckable(true);

    m_orderedListAction = addAction(
        QIcon::fromTheme("format-list-ordered"),
        "Numbered List"
    );
    m_orderedListAction->setCheckable(true);

    m_taskListAction = addAction(
        QIcon::fromTheme("checkbox"),
        "Task List"
    );
    m_taskListAction->setCheckable(true);

    m_definitionListAction = addAction(
        QIcon::fromTheme("format-list-unordered"),
        "Definition List"
    );

    addSeparator();

    // -------------------------------------------------------------------------
    // Blocks
    // -------------------------------------------------------------------------

    m_codeBlockAction = addAction(
        QIcon::fromTheme("text-x-generic"),
        "Code Block"
    );
    m_codeBlockAction->setCheckable(true);

    m_diagramBlockAction = addAction(
        QIcon::fromTheme("view-refresh"),
        "Diagram"
    );

    m_mathBlockAction = addAction(
        QIcon::fromTheme("accessories-calculator"),
        "Math Block"
    );
    m_mathBlockAction->setCheckable(true);

    m_detailsAction = addAction(
        QIcon::fromTheme("view-list-details"),
        "Collapsible Block"
    );

    m_rawHtmlAction = addAction(
        QIcon::fromTheme("text-html"),
        "HTML Block"
    );

    m_hrAction = addAction(
        QIcon::fromTheme("format-justify-fill"),
        "Horizontal Rule"
    );

    m_hardBreakAction = addAction(
        QIcon::fromTheme("go-next"),
        "Hard Break"
    );

    addSeparator();

    // -------------------------------------------------------------------------
    // Indentation
    // -------------------------------------------------------------------------

    m_decreaseIndentAction = addAction(
        QIcon::fromTheme("format-indent-less"),
        "Outdent"
    );

    m_increaseIndentAction = addAction(
        QIcon::fromTheme("format-indent-more"),
        "Indent"
    );

    addSeparator();

    // -------------------------------------------------------------------------
    // Tables
    // -------------------------------------------------------------------------

    m_insertTableAction = addAction(
        QIcon::fromTheme("insert-table"),
        "Insert Table"
    );

    m_deleteTableAction = addAction(
        QIcon::fromTheme("edit-delete"),
        "Delete Table"
    );

    m_addRowAction = addAction(
        QIcon::fromTheme("list-add"),
        "Add Row"
    );

    m_removeRowAction = addAction(
        QIcon::fromTheme("list-remove"),
        "Remove Row"
    );

    m_addColumnAction = addAction(
        QIcon::fromTheme("list-add"),
        "Add Column"
    );

    m_removeColumnAction = addAction(
        QIcon::fromTheme("list-remove"),
        "Remove Column"
    );

    m_alignTableLeftAction = addAction(
        QIcon::fromTheme("format-justify-left"),
        "Align Left"
    );

    m_alignTableCenterAction = addAction(
        QIcon::fromTheme("format-justify-center"),
        "Align Center"
    );

    m_alignTableRightAction = addAction(
        QIcon::fromTheme("format-justify-right"),
        "Align Right"
    );

    addSeparator();

    // -------------------------------------------------------------------------
    // Document
    // -------------------------------------------------------------------------

    m_footnoteAction = addAction(
        QIcon::fromTheme("footnote"),
        "Footnote"
    );

    m_tagAction = addAction(
        QIcon::fromTheme("tag"),
        "Insert Tag"
    );

    m_tocAction = addAction(
        QIcon::fromTheme("view-list-tree"),
        "Table of Contents"
    );

    m_frontmatterAction = addAction(
        QIcon::fromTheme("document-properties"),
        "Front Matter"
    );

    addSeparator();

    // -------------------------------------------------------------------------
    // View
    // -------------------------------------------------------------------------

    m_toggleSourceViewAction = addAction(
        QIcon::fromTheme("text-x-generic"),
        "Toggle Source / Preview"
    );
    m_toggleSourceViewAction->setCheckable(true);

    // -------------------------------------------------------------------------
    // Signals
    // -------------------------------------------------------------------------

    connect(
        m_headingCombo,
        QOverload<int>::of(&QComboBox::currentIndexChanged),
        this,
        &Toolbar::onHeadingLevelChanged
    );

    connect(
        m_calloutAction,
        &QAction::triggered,
        this,
        &Toolbar::onInsertCallout
    );

    connect(
        m_diagramBlockAction,
        &QAction::triggered,
        this,
        &Toolbar::onInsertDiagram
    );
}

void Toolbar::setTextEdit(TextEdit *editor)
{
    if (m_editor) {
        disconnect(m_editor, nullptr, this, nullptr);
    }

    m_editor = editor;
    if (!m_editor) return;

    connect(m_boldAction, &QAction::triggered, m_editor, &TextEdit::toggleBold);
    connect(m_italicAction, &QAction::triggered, m_editor, &TextEdit::toggleItalic);
    connect(m_strikethroughAction, &QAction::triggered, m_editor, &TextEdit::toggleStrikethrough);
    connect(m_codeSpanAction, &QAction::triggered, m_editor, &TextEdit::toggleCodeSpan);
    connect(m_highlightAction, &QAction::triggered, m_editor, &TextEdit::toggleHighlight);

    connect(m_linkAction, &QAction::triggered, m_editor, &TextEdit::insertLink);
    connect(m_wikiLinkAction, &QAction::triggered, m_editor, &TextEdit::insertWikiLink);
    connect(m_autolinkAction, &QAction::triggered, m_editor, &TextEdit::insertAutolink);
    connect(m_imageAction, &QAction::triggered, m_editor, &TextEdit::insertImage);
    connect(m_mediaAction, &QAction::triggered, m_editor, &TextEdit::insertMedia);

    connect(m_blockquoteAction, &QAction::triggered, m_editor, &TextEdit::toggleBlockQuote);
    connect(m_bulletListAction, &QAction::triggered, m_editor, &TextEdit::toggleBulletList);
    connect(m_orderedListAction, &QAction::triggered, m_editor, &TextEdit::toggleOrderedList);
    connect(m_taskListAction, &QAction::triggered, m_editor, &TextEdit::toggleTaskItem);
    connect(m_definitionListAction, &QAction::triggered, m_editor, &TextEdit::insertDefinitionList);
    connect(m_codeBlockAction, &QAction::triggered, m_editor, &TextEdit::toggleCodeBlock);
    connect(m_mathBlockAction, &QAction::triggered, m_editor, &TextEdit::toggleMathBlock);
    connect(m_detailsAction, &QAction::triggered, m_editor, &TextEdit::insertCollapsibleBlock);
    connect(m_rawHtmlAction, &QAction::triggered, m_editor, &TextEdit::insertRawHtml);
    connect(m_hrAction, &QAction::triggered, m_editor, &TextEdit::insertHorizontalRule);
    connect(m_hardBreakAction, &QAction::triggered, m_editor, &TextEdit::insertHardLineBreak);

    connect(m_increaseIndentAction, &QAction::triggered, m_editor, &TextEdit::increaseIndent);
    connect(m_decreaseIndentAction, &QAction::triggered, m_editor, &TextEdit::decreaseIndent);

    connect(m_insertTableAction, &QAction::triggered, m_editor, &TextEdit::insertTable);
    connect(m_deleteTableAction, &QAction::triggered, m_editor, &TextEdit::deleteTable);
    connect(m_addRowAction, &QAction::triggered, m_editor, &TextEdit::addTableRow);
    connect(m_removeRowAction, &QAction::triggered, m_editor, &TextEdit::removeTableRow);
    connect(m_addColumnAction, &QAction::triggered, m_editor, &TextEdit::addTableColumn);
    connect(m_removeColumnAction, &QAction::triggered, m_editor, &TextEdit::removeTableColumn);
    connect(m_alignTableLeftAction, &QAction::triggered, m_editor, &TextEdit::alignTableColumnLeft);
    connect(m_alignTableCenterAction, &QAction::triggered, m_editor, &TextEdit::alignTableColumnCenter);
    connect(m_alignTableRightAction, &QAction::triggered, m_editor, &TextEdit::alignTableColumnRight);

    connect(m_footnoteAction, &QAction::triggered, m_editor, &TextEdit::insertFootnote);
    connect(m_tagAction, &QAction::triggered, m_editor, &TextEdit::insertTag);
    connect(m_tocAction, &QAction::triggered, m_editor, &TextEdit::insertTableOfContents);
    connect(m_frontmatterAction, &QAction::triggered, m_editor, &TextEdit::toggleFrontmatter);

    connect(m_toggleSourceViewAction, &QAction::triggered, m_editor, &TextEdit::toggleSourceMode);

    connect(m_editor, &TextEdit::formatChanged, this, &Toolbar::updateActionsState);
    updateActionsState();
}

void Toolbar::updateActionsState()
{
    if (!m_editor) return;

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

    int level = m_editor->currentHeadingLevel();
    int comboIndex = m_headingCombo->findData(level);
    if (comboIndex != -1) {
        QSignalBlocker blocker(m_headingCombo);
        m_headingCombo->setCurrentIndex(comboIndex);
    }

    bool inTable = m_editor->isInsideTable();
    m_deleteTableAction->setEnabled(inTable);
    m_addRowAction->setEnabled(inTable);
    m_removeRowAction->setEnabled(inTable);
    m_addColumnAction->setEnabled(inTable);
    m_removeColumnAction->setEnabled(inTable);
    m_alignTableLeftAction->setEnabled(inTable);
    m_alignTableCenterAction->setEnabled(inTable);
    m_alignTableRightAction->setEnabled(inTable);

    m_toggleSourceViewAction->setChecked(m_editor->isSourceMode());
}

void Toolbar::onHeadingLevelChanged(int index)
{
    if (!m_editor) return;
    int level = m_headingCombo->itemData(index).toInt();
    m_editor->setHeadingLevel(level);
}

void Toolbar::onInsertCallout()
{
    if (!m_editor) return;
    QStringList types = {"NOTE", "TIP", "WARNING", "CAUTION", "IMPORTANT"};
    bool ok;
    QString type = QInputDialog::getItem(this, "Insert Callout", "Type:", types, 0, false, &ok);
    if (ok && !type.isEmpty()) {
        m_editor->insertCallout(type);
    }
}

void Toolbar::onInsertDiagram()
{
    if (!m_editor) return;
    QStringList engineTypes = {"mermaid", "plantuml"};
    bool ok;
    QString type = QInputDialog::getItem(this, "Insert Diagram", "Engine:", engineTypes, 0, false, &ok);
    if (ok && !type.isEmpty()) {
        m_editor->insertDiagramBlock(type);
    }
}