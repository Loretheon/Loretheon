#include "../../include/text/DocumentArea.h"

#include "../../include/app/DocumentManager.h"

#include "../../include/text/TextEdit.h"
#include "../../include/text/TextWidget.h"
#include "../../include/text/Toolbar.h"
#include "../../include/text/media/MediaPane.h"

#include "../../include/ai/edit/EditSession.h"

#include <QFileInfo>
#include <QHBoxLayout>
#include <QSignalBlocker>
#include <QTabBar>
#include <QToolButton>
#include <QVBoxLayout>

namespace {

constexpr int kTabButtonSize = 20;

QString tabLabelFor(TextDocument *document) {
  if (!document) {
    return {};
  }

  const QString path = document->filePath();

  QString name = path.isEmpty() ? QObject::tr("Untitled")
                                : QFileInfo(path).fileName();

  if (document->isModified()) {
    name.prepend(QStringLiteral("* "));
  }

  return name;
}

} // namespace

DocumentArea::DocumentArea(DocumentManager *manager, QWidget *parent)
    : QStackedWidget(parent), m_manager(manager) {
  m_tabs = new QTabWidget(this);
  m_tabs->setTabsClosable(true);
  m_tabs->setMovable(true);
  m_tabs->setDocumentMode(true);

  m_mediaPane = new MediaPane(this);

  m_pageTabs = addWidget(m_tabs);
  m_pageMedia = addWidget(m_mediaPane);

  setCurrentIndex(m_pageTabs);

  connect(m_tabs, &QTabWidget::currentChanged, this,
          &DocumentArea::onCurrentChanged);

  connect(m_tabs, &QTabWidget::tabCloseRequested, this, [this](int index) {
    if (!m_manager) {
      return;
    }

    auto *page = qobject_cast<TextWidget *>(m_tabs->widget(index));

    if (!page) {
      return;
    }

    for (auto it = m_widgets.constBegin(); it != m_widgets.constEnd(); ++it) {
      if (it.value() == page) {
        m_manager->closeDocument(it.key());
        return;
      }
    }
  });

  if (m_manager) {
    connect(m_manager, &DocumentManager::documentOpened, this,
            &DocumentArea::onDocumentOpened);

    connect(m_manager, &DocumentManager::documentClosed, this,
            &DocumentArea::onDocumentClosed);

    connect(m_manager, &DocumentManager::mediaFileRequested, this,
            &DocumentArea::showMediaFile);
  }
}

TextEdit *DocumentArea::currentEditor() const {
  auto *page = currentTextWidget();
  return page ? page->editor() : nullptr;
}

TextWidget *DocumentArea::currentTextWidget() const {
  return qobject_cast<TextWidget *>(m_tabs->currentWidget());
}

TextDocument *DocumentArea::currentDocument() const {
  return m_manager ? m_manager->currentDocument() : nullptr;
}

void DocumentArea::setEditSession(EditSession *session) {
  m_session = session;

  for (TextWidget *page : std::as_const(m_widgets)) {
    if (page) {
      page->setPreviewSession(session);
    }
  }

  wireSessionToEditor(session, currentEditor());
}

void DocumentArea::wireSessionToEditor(EditSession *session,
                                       TextEdit *editor) {
  if (m_sessionEditor == editor && m_session == session) {
    return;
  }

  if (m_sessionEditor) {
    disconnect(m_sessionEditor, nullptr, session, nullptr);
    disconnect(session, nullptr, m_sessionEditor, nullptr);

    m_sessionEditor = nullptr;
  }

  if (!session || !editor) {
    return;
  }

  m_sessionEditor = editor;

  connect(session, &EditSession::pendingEditStarted, editor,
          [editor](const PendingEdit &edit) { editor->showPendingEdit(edit); });

  connect(session, &EditSession::pendingEditUpdated, editor,
          [editor](const PendingEdit &edit) { editor->updatePendingEdit(edit); });

  connect(session, &EditSession::pendingEditFinished, editor,
          [editor](const PendingEdit &edit) { editor->updatePendingEdit(edit); });

  connect(session, &EditSession::pendingEditsChanged, editor,
          [editor]() { editor->refreshPendingEdits(); });

  connect(session, &EditSession::reviewReady, editor,
          [editor]() { editor->refreshPendingEdits(); });

  connect(session, &EditSession::planReady, editor,
          [editor](const QVector<EditCommand> &) {
            editor->refreshPendingEdits();
          });

  connect(session, &EditSession::aborted, editor,
          [editor]() { editor->clearPendingEdits(); });

  connect(editor, &TextEdit::acceptPendingEditRequested, session,
          [session](int id) { session->acceptPendingEdit(id); });

  connect(editor, &TextEdit::rejectPendingEditRequested, session,
          [session](int id) { session->rejectPendingEdit(id); });

  connect(editor, &TextEdit::acceptAllPendingEditsRequested, session,
          [session]() {
            session->acceptAllPendingEdits();
            session->applyAcceptedPendingEdits();
          });

  connect(editor, &TextEdit::rejectAllPendingEditsRequested, session,
          [session]() { session->rejectAllPendingEdits(); });

  connect(editor, &TextEdit::autoAcceptChanged, session,
          [session](bool enabled) {
            if (!enabled) {
              return;
            }

            session->acceptAllPendingEdits();
            session->applyAcceptedPendingEdits();
          });
}

void DocumentArea::setThemeTokens(const ThemeTokens &tokens) {
  m_tokens = tokens;

  for (TextWidget *page : std::as_const(m_widgets)) {
    if (page) {
      page->setThemeTokens(tokens);
    }
  }
}

void DocumentArea::showMediaFile(const QString &absolutePath) {
  if (!m_mediaPane) {
    return;
  }

  m_mediaPane->load(absolutePath);
  setCurrentIndex(m_pageMedia);
}

void DocumentArea::showTextTabs() {
  setCurrentIndex(m_pageTabs);
}

TextWidget *DocumentArea::widgetForDocument(TextDocument *document) const {
  return m_widgets.value(document, nullptr);
}

int DocumentArea::indexForDocument(TextDocument *document) const {
  auto *page = widgetForDocument(document);

  if (!page) {
    return -1;
  }

  return m_tabs->indexOf(page);
}

void DocumentArea::installTabButtons(TextWidget *page) {
  if (!page || !m_tabs) {
    return;
  }

  const int index = m_tabs->indexOf(page);

  if (index < 0) {
    return;
  }

  // The tab bar allows exactly one widget per side. Build a small
  // container holding the view-mode toggle followed by the close
  // button, and set that container as the tab's right-side button.
  // The default close button is replaced by this container; the
  // container's own close button fires the close path.

  auto *container = new QWidget(m_tabs);
  container->setObjectName(QStringLiteral("tabButtonContainer"));

  auto *layout = new QHBoxLayout(container);
  layout->setContentsMargins(0, 0, 0, 0);
  layout->setSpacing(2);

  auto *viewModeButton = new QToolButton(container);
  viewModeButton->setObjectName(QStringLiteral("tabViewModeButton"));
  viewModeButton->setText(QStringLiteral("◧"));
  viewModeButton->setToolTip(tr("Edit / View"));
  viewModeButton->setCursor(Qt::PointingHandCursor);
  viewModeButton->setFocusPolicy(Qt::NoFocus);
  viewModeButton->setAutoRaise(true);
  viewModeButton->setFixedSize(kTabButtonSize, kTabButtonSize);
  viewModeButton->setCheckable(true);

  auto *closeButton = new QToolButton(container);
  closeButton->setObjectName(QStringLiteral("tabCloseButton"));
  closeButton->setText(QStringLiteral("✕"));
  closeButton->setToolTip(tr("Close"));
  closeButton->setCursor(Qt::PointingHandCursor);
  closeButton->setFocusPolicy(Qt::NoFocus);
  closeButton->setAutoRaise(true);
  closeButton->setFixedSize(kTabButtonSize, kTabButtonSize);

  layout->addWidget(viewModeButton);
  layout->addWidget(closeButton);

  connect(viewModeButton, &QToolButton::toggled, this,
          [this, page](bool checked) {
            if (!page) {
              return;
            }

            page->setViewMode(checked);

            if (page->editor() && page->editor()->toolbar()) {
              page->editor()->toolbar()->setVisible(!checked);
            }
          });

  connect(closeButton, &QToolButton::clicked, this, [this, page]() {
    if (!page || !m_manager) {
      return;
    }

    for (auto it = m_widgets.constBegin(); it != m_widgets.constEnd(); ++it) {
      if (it.value() == page) {
        m_manager->closeDocument(it.key());
        return;
      }
    }
  });

  m_tabs->tabBar()->setTabButton(index, QTabBar::RightSide, container);

  if (page->editor() && page->editor()->toolbar()) {
    page->editor()->toolbar()->setVisible(!page->isViewMode());
  }

  if (page->isViewMode()) {
    viewModeButton->blockSignals(true);
    viewModeButton->setChecked(true);
    viewModeButton->blockSignals(false);
  }
}

void DocumentArea::syncTabButtons() {
  for (TextWidget *page : std::as_const(m_widgets)) {
    if (!page) {
      continue;
    }

    const int index = m_tabs->indexOf(page);

    if (index < 0) {
      continue;
    }

    QWidget *container =
        m_tabs->tabBar()->tabButton(index, QTabBar::RightSide);

    if (!container || container->objectName() !=
                          QStringLiteral("tabButtonContainer")) {
      installTabButtons(page);
      continue;
    }

    auto *viewModeButton = container->findChild<QToolButton *>(
        QStringLiteral("tabViewModeButton"));

    if (!viewModeButton) {
      installTabButtons(page);
      continue;
    }

    const bool viewMode = page->isViewMode();

    viewModeButton->blockSignals(true);
    viewModeButton->setChecked(viewMode);
    viewModeButton->blockSignals(false);

    if (page->editor() && page->editor()->toolbar()) {
      page->editor()->toolbar()->setVisible(!viewMode);
    }
  }
}

void DocumentArea::onDocumentOpened(TextDocument *document) {
  if (!document || m_widgets.contains(document)) {
    return;
  }

  auto *page = new TextWidget(m_tabs);

  page->setActiveDocument(document);
  page->setPreviewSession(m_session);
  page->setThemeTokens(m_tokens);

  connect(page, &TextWidget::openDocumentRequested, this,
          &DocumentArea::openDocumentRequested);

  connect(page, &TextWidget::statusMessage, this,
          &DocumentArea::statusMessage);

  const int index = m_tabs->addTab(page, tabLabelFor(document));
  m_widgets.insert(document, page);

  installTabButtons(page);

  m_tabs->setCurrentIndex(index);
  showTextTabs();

  connect(document, &QTextDocument::contentsChanged, this,
          [this, document]() {
            const int i = indexForDocument(document);
            if (i >= 0) {
              m_tabs->setTabText(i, tabLabelFor(document));
            }
          });
}

void DocumentArea::onDocumentClosed(TextDocument *document) {
  auto *page = widgetForDocument(document);

  if (!page) {
    return;
  }

  if (m_sessionEditor == page->editor()) {
    wireSessionToEditor(m_session, nullptr);
  }

  const int index = m_tabs->indexOf(page);

  if (index >= 0) {
    if (QWidget *container =
            m_tabs->tabBar()->tabButton(index, QTabBar::RightSide)) {
      container->deleteLater();
    }
  }

  m_widgets.remove(document);

  m_tabs->removeTab(index);
  page->deleteLater();

  if (m_manager && m_manager->currentDocument() &&
      m_manager->currentDocument() != document) {
    const int newIndex = indexForDocument(m_manager->currentDocument());

    if (newIndex >= 0 && newIndex != m_tabs->currentIndex()) {
      QSignalBlocker blocker(m_tabs);
      m_tabs->setCurrentIndex(newIndex);
    }
  }
}

void DocumentArea::onCurrentChanged(int index) {
  if (m_syncing || !m_manager) {
    return;
  }

  auto *page = qobject_cast<TextWidget *>(m_tabs->widget(index));

  if (!page) {
    wireSessionToEditor(m_session, nullptr);

    emit currentEditorChanged(nullptr);
    emit currentTextWidgetChanged(nullptr);
    return;
  }

  TextDocument *document = nullptr;

  for (auto it = m_widgets.constBegin(); it != m_widgets.constEnd(); ++it) {
    if (it.value() == page) {
      document = it.key();
      break;
    }
  }

  if (!document) {
    return;
  }

  m_syncing = true;
  m_manager->setCurrentDocument(document);
  m_syncing = false;

  wireSessionToEditor(m_session, page->editor());

  emit currentEditorChanged(page->editor());
  emit currentTextWidgetChanged(page);
}