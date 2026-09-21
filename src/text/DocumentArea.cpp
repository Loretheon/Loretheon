#include "../../include/text/DocumentArea.h"

#include "../../include/app/DocumentManager.h"

#include "../../include/text/TextEdit.h"
#include "../../include/text/TextWidget.h"
#include "../../include/text/media/MediaPane.h"

#include "../../include/ai/edit/EditSession.h"

#include <QFileInfo>
#include <QTabBar>
#include <QVBoxLayout>

namespace {

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

  const int index = m_tabs->indexOf(page);

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

  emit currentEditorChanged(page->editor());
  emit currentTextWidgetChanged(page);
}