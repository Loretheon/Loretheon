#include "../../include/text/Toolbar.h"

#include "TextEdit.h"

#include <QApplication>
#include <QGuiApplication>
#include <QInputDialog>
#include <QMenu>
#include <QPalette>
#include <QSignalBlocker>
#include <qlayout.h>

namespace {

struct ActionBinding {
  QAction *action;
  void (TextEdit::*handler)();
};

} // namespace

static QWidget * createSpacer(QWidget *parent) {
  auto *spacer = new QWidget(parent);
  spacer->setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Preferred);
  spacer->setMinimumWidth(5);
  return spacer;
}

static QAction * addExpandingAction(QToolBar *toolbar, const QIcon &icon,
                                    const QString &text = QString()) {
  QAction *action = toolbar->addAction(icon, text);

  if (QWidget *button = toolbar->widgetForAction(action)) {
    QSizePolicy policy = button->sizePolicy();
    policy.setHorizontalPolicy(QSizePolicy::Expanding);
    policy.setHorizontalStretch(1);
    button->setSizePolicy(policy);
  }

  return action;
}

QIcon Toolbar::createThemedIcon(const QString &symbolName) {
  // The AppImage does not carry a system icon theme, so QIcon::fromTheme
  // returns null for every name on a machine that has no theme installed.
  // The icons are compiled into the binary through
  // resources/icons/resources.qrc, so the resource is the canonical
  // source and the system theme is the fallback for names not yet
  // bundled.

  const QIcon resource =
      QIcon(QStringLiteral(":/icons/dark/%1.svg").arg(symbolName));

  if (!resource.isNull() && !resource.availableSizes().isEmpty()) {
    return resource;
  }

  const QIcon themed = QIcon::fromTheme(symbolName);

  if (!themed.isNull()) {
    return themed;
  }

  return QIcon(QStringLiteral(":/icons/dark/document-properties.svg"));
}

Toolbar::Toolbar(QWidget *parent) : QToolBar(parent) {
  setMovable(false);
  setIconSize(QSize(18, 18));
  setToolButtonStyle(Qt::ToolButtonIconOnly);
  layout()->setSpacing(6);
  setupToolbarStyle();

  m_boldAction = addExpandingAction(this, createThemedIcon("format-text-bold"));
  m_boldAction->setCheckable(true);
  m_boldAction->setShortcut(Qt::CTRL | Qt::Key_B);
  m_boldAction->setToolTip(tr("Bold (Ctrl+B)"));

  m_italicAction = addExpandingAction(this, createThemedIcon("format-text-italic"));
  m_italicAction->setCheckable(true);
  m_italicAction->setShortcut(Qt::CTRL | Qt::Key_I);
  m_italicAction->setToolTip(tr("Italic (Ctrl+I)"));

  m_strikethroughAction = addExpandingAction(this, createThemedIcon("format-text-strikethrough"));
  m_strikethroughAction->setCheckable(true);
  m_strikethroughAction->setToolTip(tr("Strikethrough"));

  m_codeSpanAction = addExpandingAction(this, createThemedIcon("format-text-code"));
  m_codeSpanAction->setCheckable(true);
  m_codeSpanAction->setShortcut(Qt::CTRL | Qt::Key_Agrave);
  m_codeSpanAction->setToolTip(tr("Inline Code (Ctrl+`)"));

  m_highlightAction = addExpandingAction(this, createThemedIcon("format-text-highlight"));
  m_highlightAction->setCheckable(true);
  m_highlightAction->setToolTip(tr("Highlight"));

  addWidget(createSpacer(this));

  m_linkAction = addExpandingAction(this, createThemedIcon("insert-link"));
  m_linkAction->setShortcut(Qt::CTRL | Qt::Key_K);
  m_linkAction->setToolTip(tr("Link (Ctrl+K)"));

  m_wikiLinkAction = addExpandingAction(this, createThemedIcon("insert-link"));
  m_wikiLinkAction->setToolTip(tr("Wiki Link"));

  m_autolinkAction = addExpandingAction(this, createThemedIcon("link"));
  m_autolinkAction->setToolTip(tr("Autolink"));

  m_imageAction = addExpandingAction(this, createThemedIcon("insert-image"));
  m_imageAction->setToolTip(tr("Image"));

  m_mediaAction = addExpandingAction(this, createThemedIcon("video-x-generic"));
  m_mediaAction->setToolTip(tr("Media"));

  addWidget(createSpacer(this));

  m_headingCombo = new QComboBox(this);
  m_headingCombo->addItem(tr("¶"), 0);

  for (int level = 1; level <= 6; ++level) {
    m_headingCombo->addItem(QString("H%1").arg(level), level);
  }

  m_headingCombo->setToolTip(tr("Heading Level"));
  m_headingCombo->setMaximumWidth(70);
  m_headingCombo->setMinimumWidth(55);

  addWidget(m_headingCombo);

  m_blockquoteAction = addExpandingAction(this, createThemedIcon("format-quote"));
  m_blockquoteAction->setCheckable(true);
  m_blockquoteAction->setToolTip(tr("Quote"));

  m_calloutAction = addExpandingAction(this, createThemedIcon("dialog-information"));
  m_calloutAction->setToolTip(tr("Callout"));

  addWidget(createSpacer(this));

  m_bulletListAction = addExpandingAction(this, createThemedIcon("format-list-unordered"));
  m_bulletListAction->setCheckable(true);
  m_bulletListAction->setToolTip(tr("Bullet List"));

  m_orderedListAction = addExpandingAction(this, createThemedIcon("format-list-ordered"));
  m_orderedListAction->setCheckable(true);
  m_orderedListAction->setToolTip(tr("Ordered List"));

  m_taskListAction = addExpandingAction(this, createThemedIcon("checkbox"));
  m_taskListAction->setCheckable(true);
  m_taskListAction->setToolTip(tr("Task List"));

  m_definitionListAction = addExpandingAction(this, createThemedIcon("format-list-unordered"));
  m_definitionListAction->setToolTip(tr("Definition List"));

  addWidget(createSpacer(this));

  m_codeBlockAction = addExpandingAction(this, createThemedIcon("text-x-generic"));
  m_codeBlockAction->setCheckable(true);
  m_codeBlockAction->setToolTip(tr("Code Block"));

  m_diagramBlockAction = addExpandingAction(this, createThemedIcon("view-refresh"));
  m_diagramBlockAction->setToolTip(tr("Diagram"));

  m_mathBlockAction = addExpandingAction(this, createThemedIcon("accessories-calculator"));
  m_mathBlockAction->setCheckable(true);
  m_mathBlockAction->setToolTip(tr("Math Block"));

  m_detailsAction = addExpandingAction(this, createThemedIcon("view-list-details"));
  m_detailsAction->setToolTip(tr("Collapsible"));

  m_rawHtmlAction = addExpandingAction(this, createThemedIcon("text-html"));
  m_rawHtmlAction->setToolTip(tr("HTML Block"));

  m_hrAction = addExpandingAction(this, createThemedIcon("format-justify-fill"));
  m_hrAction->setToolTip(tr("Divider"));

  m_hardBreakAction = addExpandingAction(this, createThemedIcon("go-next"));
  m_hardBreakAction->setToolTip(tr("Line Break"));

  addWidget(createSpacer(this));

  m_decreaseIndentAction = addExpandingAction(this, createThemedIcon("format-indent-less"));
  m_decreaseIndentAction->setShortcut(Qt::SHIFT | Qt::Key_Tab);
  m_decreaseIndentAction->setToolTip(tr("Outdent (Shift+Tab)"));

  m_increaseIndentAction = addExpandingAction(this, createThemedIcon("format-indent-more"));
  m_increaseIndentAction->setShortcut(Qt::Key_Tab);
  m_increaseIndentAction->setToolTip(tr("Indent (Tab)"));

  addWidget(createSpacer(this));

  m_insertTableAction = addExpandingAction(this, createThemedIcon("insert-table"));
  m_insertTableAction->setToolTip(tr("Insert Table"));

  m_deleteTableAction = addExpandingAction(this, createThemedIcon("edit-delete"));
  m_deleteTableAction->setToolTip(tr("Delete Table"));

  m_addRowAction = addExpandingAction(this, createThemedIcon("list-add"));
  m_addRowAction->setToolTip(tr("Add Row"));

  m_removeRowAction = addExpandingAction(this, createThemedIcon("list-remove"));
  m_removeRowAction->setToolTip(tr("Remove Row"));

  m_addColumnAction = addExpandingAction(this, createThemedIcon("list-add"));
  m_addColumnAction->setToolTip(tr("Add Column"));

  m_removeColumnAction = addExpandingAction(this, createThemedIcon("list-remove"));
  m_removeColumnAction->setToolTip(tr("Remove Column"));

  m_alignTableLeftAction = addExpandingAction(this, createThemedIcon("format-justify-left"));
  m_alignTableLeftAction->setToolTip(tr("Align Left"));

  m_alignTableCenterAction = addExpandingAction(this, createThemedIcon("format-justify-center"));
  m_alignTableCenterAction->setToolTip(tr("Align Center"));

  m_alignTableRightAction = addExpandingAction(this, createThemedIcon("format-justify-right"));
  m_alignTableRightAction->setToolTip(tr("Align Right"));

  addWidget(createSpacer(this));

  m_footnoteAction = addExpandingAction(this, createThemedIcon("footnote"));
  m_footnoteAction->setToolTip(tr("Footnote"));

  m_tagAction = addExpandingAction(this, createThemedIcon("tag"));
  m_tagAction->setToolTip(tr("Tag"));

  m_tocAction = addExpandingAction(this, createThemedIcon("view-list-tree"));
  m_tocAction->setToolTip(tr("Table of Contents"));

  m_frontmatterAction = addExpandingAction(this, createThemedIcon("document-properties"));
  m_frontmatterAction->setToolTip(tr("Metadata"));

  addWidget(createSpacer(this));

  m_toggleSourceViewAction = addExpandingAction(this, createThemedIcon("text-x-generic"));
  m_toggleSourceViewAction->setCheckable(true);
  m_toggleSourceViewAction->setShortcut(Qt::CTRL | Qt::SHIFT | Qt::Key_V);
  m_toggleSourceViewAction->setToolTip(tr("Source/Preview (Ctrl+Shift+V)"));

  connect(m_headingCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
          this, &Toolbar::onHeadingLevelChanged);

  connect(m_calloutAction, &QAction::triggered, this,
          &Toolbar::onInsertCallout);

  connect(m_diagramBlockAction, &QAction::triggered, this,
          &Toolbar::onInsertDiagram);
}

void Toolbar::setupToolbarStyle() {
  QPalette palette = QApplication::palette();
  QString textColor = palette.text().color().name();
  QString bgColor = palette.base().color().name();

  setStyleSheet(
      "QToolBar {"
      "  border: none;"
      "  background-color: "+ bgColor + ";"
      "  padding: 2px 4px;"
      "  spacing: 1px;"
      "}"
      "QToolButton {"
      "  border: none;"
      "  border-radius: 3px;"
      "  padding: 3px;"
      "  margin: 0px;"
      "  background-color: transparent;"
      "}"
      "QToolButton:hover {"
      "  background-color: rgba(128, 128, 128, 0.15);"
      "}"
      "QToolButton:pressed,"
      "QToolButton:checked {"
      "  background-color: rgba(128, 128, 128, 0.3);"
      "}"
      "QComboBox {"
      "  border: 1px solid rgba(128, 128, 128, 0.3);"
      "  border-radius: 3px;"
      "  padding: 2px 4px;"
      "  background-color: transparent;"
      "  color: " + textColor + ";"
      "}"
      "QComboBox:hover {"
      "  border: 1px solid rgba(128, 128, 128, 0.5);"
      "}"
      "QComboBox::drop-down {"
      "  border: none;"
      "  padding-right: 3px;"
      "}"
      "QComboBox::down-arrow {"
      "  width: 12px;"
      "  height: 8px;"
      "}"
  );
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