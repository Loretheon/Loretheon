#include "../../include/text/DocumentArea.h"

#include "../../include/app/DocumentManager.h"

#include "../../include/text/TextEdit.h"
#include "../../include/text/TextWidget.h"

#include "../../include/ai/edit/EditSession.h"

#include <QFileInfo>
#include <QTabBar>

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
    : QTabWidget(parent), m_manager(manager) {
  setTabsClosable(true);
  setMovable(true);
  setDocumentMode(true);

  connect(this, &QTabWidget::currentChanged, this,
          &DocumentArea::onCurrentChanged);

  connect(this, &QTabWidget::tabCloseRequested, this, [this](int index) {
    if (!m_manager) {
      return;
    }

    auto *page = qobject_cast<TextWidget *>(QTabWidget::widget(index));

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
  }
}

TextEdit *DocumentArea::currentEditor() const {
  auto *page = currentTextWidget();
  return page ? page->editor() : nullptr;
}

TextWidget *DocumentArea::currentTextWidget() const {
  return qobject_cast<TextWidget *>(currentWidget());
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

TextWidget *DocumentArea::widgetForDocument(TextDocument *document) const {
  return m_widgets.value(document, nullptr);
}

int DocumentArea::indexForDocument(TextDocument *document) const {
  auto *page = widgetForDocument(document);

  if (!page) {
    return -1;
  }

  return indexOf(page);
}

void DocumentArea::onDocumentOpened(TextDocument *document) {
  if (!document || m_widgets.contains(document)) {
    return;
  }

  auto *page = new TextWidget(this);

  page->setActiveDocument(document);
  page->setPreviewSession(m_session);
  page->setThemeTokens(m_tokens);

  connect(page, &TextWidget::openDocumentRequested, this,
          &DocumentArea::openDocumentRequested);

  connect(page, &TextWidget::statusMessage, this,
          &DocumentArea::statusMessage);

  const int index = addTab(page, tabLabelFor(document));
  m_widgets.insert(document, page);

  setCurrentIndex(index);

  connect(document, &QTextDocument::contentsChanged, this,
          [this, document]() {
            const int i = indexForDocument(document);
            if (i >= 0) {
              setTabText(i, tabLabelFor(document));
            }
          });
}

void DocumentArea::onDocumentClosed(TextDocument *document) {
  auto *page = widgetForDocument(document);

  if (!page) {
    return;
  }

  const int index = indexOf(page);

  m_widgets.remove(document);

  removeTab(index);
  page->deleteLater();

  if (m_manager && m_manager->currentDocument() &&
      m_manager->currentDocument() != document) {
    const int newIndex = indexForDocument(m_manager->currentDocument());

    if (newIndex >= 0 && newIndex != currentIndex()) {
      QSignalBlocker blocker(this);
      setCurrentIndex(newIndex);
    }
  }
}

void DocumentArea::onCurrentChanged(int index) {
  if (m_syncing || !m_manager) {
    return;
  }

  auto *page = qobject_cast<TextWidget *>(QTabWidget::widget(index));

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