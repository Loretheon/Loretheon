#include "TextWidget.h"

#include "DiagramDocument.h"
#include "DiagramToolbar.h"
#include "DiagramView.h"
#include "GraphvizRenderer.h"
#include "MermaidRenderer.h"
#include "PlantUmlRenderer.h"
#include "TextBrowser.h"
#include "TextEdit.h"
#include "TextDocument.h"

#include "preview/PreviewController.h"
#include "preview/PreviewPane.h"

#include "../ai/edit/EditSession.h"

#include <QActionGroup>
#include <QApplication>
#include <QClipboard>
#include <QContextMenuEvent>
#include <QDebug>
#include <QDesktopServices>
#include <QFileInfo>
#include <QMenu>
#include <QProcess>
#include <QShowEvent>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QTextDocument>
#include <QUrl>
#include <QVBoxLayout>

TextWidget::TextWidget(QWidget *parent)
    : QWidget(parent),
      textEdit(new TextEdit(this)),
      previewPane(new PreviewPane(this)),
      editorStack(new QStackedWidget(this)),
      textBrowser(new TextBrowser(this)),
      viewStack(new QStackedWidget(this)),
      diagramPage(new QWidget(this)),
      svgView(new DiagramView(this)),
      diagramToolbar(new DiagramToolbar(this)),
      diagramDoc(new DiagramDocument(this)),
      previewDocument(new QTextDocument(this)),
      graphvizRenderer(new GraphvizRenderer(this)),
      plantUmlRenderer(new PlantUmlRenderer(this)),
      mermaidRenderer(new MermaidRenderer(this)) {
  editorStack->addWidget(textEdit);
  editorStack->addWidget(previewPane);

  viewStack->addWidget(textBrowser);
  viewStack->addWidget(diagramPage);

  outerStack = new QStackedWidget(this);
  outerStack->addWidget(editorStack);
  outerStack->addWidget(viewStack);
  outerStack->setCurrentIndex(0);

  auto *pageLay = new QVBoxLayout(diagramPage);
  pageLay->setContentsMargins(0, 0, 0, 0);
  pageLay->setSpacing(0);
  pageLay->addWidget(diagramToolbar);
  pageLay->addWidget(svgView, 1);

  textBrowser->setDocument(previewDocument);
  svgView->setDocument(diagramDoc);

  previewController =
      new PreviewController(textEdit, nullptr, previewPane, this);

  auto *mainLay = new QVBoxLayout(this);
  mainLay->setContentsMargins(0, 0, 0, 0);
  mainLay->setSpacing(0);
  mainLay->addWidget(outerStack, 1);

  connect(diagramToolbar, &DiagramToolbar::zoomInRequested, svgView,
          &DiagramView::zoomIn);
  connect(diagramToolbar, &DiagramToolbar::zoomOutRequested, svgView,
          &DiagramView::zoomOut);
  connect(diagramToolbar, &DiagramToolbar::zoomResetRequested, svgView,
          &DiagramView::zoomReset);
  connect(diagramToolbar, &DiagramToolbar::fitRequested, svgView,
          &DiagramView::zoomFit);
  connect(diagramToolbar, &DiagramToolbar::zoomToRequested, svgView,
          &DiagramView::setZoom);
  connect(svgView, &DiagramView::zoomChanged, diagramToolbar,
          &DiagramToolbar::setZoom);

  connect(graphvizRenderer, &GraphvizRenderer::svgReady, this,
          [this](const QString &svg) {
            if (svg.isEmpty()) {
              diagramDoc->clear();
              diagramToolbar->setActionsEnabled(false);
              return;
            }
            diagramDoc->setSvg(svg);
            diagramToolbar->setActionsEnabled(true);
          });

  connect(graphvizRenderer, &GraphvizRenderer::renderFailed, this,
          [this](const QString &err) {
            qWarning() << "Graphviz:" << err;
            diagramDoc->clear();
            diagramToolbar->setActionsEnabled(false);
            emit statusMessage(tr("Graphviz: %1").arg(err), 5000);
          });

  connect(plantUmlRenderer, &PlantUmlRenderer::svgReady, this,
          [this](const QString &svg) {
            if (svg.isEmpty()) {
              diagramDoc->clear();
              diagramToolbar->setActionsEnabled(false);
              return;
            }
            diagramDoc->setSvg(svg);
            diagramToolbar->setActionsEnabled(true);
          });

  connect(plantUmlRenderer, &PlantUmlRenderer::renderFailed, this,
          [this](const QString &err) {
            qWarning() << "PlantUML:" << err;
            diagramDoc->clear();
            diagramToolbar->setActionsEnabled(false);
            emit statusMessage(tr("PlantUML: %1").arg(err), 5000);
          });

  connect(mermaidRenderer, &MermaidRenderer::svgReady, this,
          [this](const QString &svg) {
            if (svg.isEmpty()) {
              diagramDoc->clear();
              diagramToolbar->setActionsEnabled(false);
              return;
            }
            diagramDoc->setSvg(svg);
            diagramToolbar->setActionsEnabled(true);
          });

  connect(mermaidRenderer, &MermaidRenderer::renderFailed, this,
          [this](const QString &err) {
            qWarning() << "Mermaid:" << err;
            diagramDoc->clear();
            diagramToolbar->setActionsEnabled(false);
            emit statusMessage(tr("Mermaid: %1").arg(err), 5000);
          });

  connect(svgView, &DiagramView::elementActivated, this,
          [this](const QString &, const QString &target,
                 DiagramDocument::NodeKind kind) {
            if (target.isEmpty()) return;
            if (kind == DiagramDocument::NodeKind::External) {
              QDesktopServices::openUrl(QUrl(target));
              return;
            }
            if (kind == DiagramDocument::NodeKind::Reference) {
              emit openDocumentRequested(target);
            }
          });

  connect(svgView, &DiagramView::elementClicked, this,
          &TextWidget::onElementClicked);

  connect(svgView, &DiagramView::elementRightClicked, this,
          [this](const QString &id, const QString &name,
                 const QPoint &globalPos) {
            showNodeContextMenu(id, name, globalPos);
          });

  diagramToolbar->setActionsEnabled(false);
  diagramToolbar->setZoom(1.0);
}

void TextWidget::showEvent(QShowEvent *event) {
  QWidget::showEvent(event);

  if (activeDocument) {
    syncPreview();
  }
}

bool TextWidget::isViewMode() const {
  return outerStack && outerStack->currentIndex() == 1;
}

void TextWidget::setViewMode(bool viewMode) {
  if (!outerStack)
    return;

  outerStack->setCurrentIndex(viewMode ? 1 : 0);
}

void TextWidget::contextMenuEvent(QContextMenuEvent *event) {
  QMenu menu(this);

  QAction *editAct = menu.addAction(tr("Edit"));
  editAct->setCheckable(true);
  editAct->setChecked(!isViewMode());

  QAction *viewAct = menu.addAction(tr("View"));
  viewAct->setCheckable(true);
  viewAct->setChecked(isViewMode());

  QActionGroup *modeGroup = new QActionGroup(&menu);
  modeGroup->addAction(editAct);
  modeGroup->addAction(viewAct);
  modeGroup->setExclusive(true);

  menu.addSeparator();

  QAction *chosen = menu.exec(event->globalPos());

  if (chosen == editAct)
    setViewMode(false);
  else if (chosen == viewAct)
    setViewMode(true);
}

void TextWidget::setWorkstationMode(bool on) {
  if (m_workstationMode == on)
    return;

  m_workstationMode = on;

  if (textEdit) {
    textEdit->setToolbarVisible(!on);
  }

  if (on) {
    setViewMode(true);
  }
}

void TextWidget::setPreviewSession(EditSession *session) {
  if (previewController) {
    previewController->setSession(session);
  }
}

void TextWidget::activatePreview(bool active) {
  if (!previewController) {
    return;
  }
  previewController->setPreviewActive(active);
}

void TextWidget::clearContextScopes() {
  if (textEdit) {
    textEdit->clearHighlightedScopes();
  }
}

void TextWidget::setContextScopes(const QStringList &scopeIds) {
  if (textEdit) {
    textEdit->setHighlightedScopes(scopeIds);
  }
}

void TextWidget::setThemeTokens(const ThemeTokens &tokens) {
  m_tokens = tokens;
  applyThemeToRenderers();
  refreshActiveRenderer();
}

void TextWidget::applyThemeToRenderers() {
  if (graphvizRenderer) graphvizRenderer->setThemeTokens(m_tokens);
  if (plantUmlRenderer) plantUmlRenderer->setThemeTokens(m_tokens);
  if (mermaidRenderer) mermaidRenderer->setThemeTokens(m_tokens);
}

void TextWidget::refreshActiveRenderer() {
  if (!activeDocument)
    return;

  // The Markdown and plain-text previews are QTextDocument and inherit
  // the widget palette through QSS. They do not need a re-render.

  switch (activeDocument->type()) {
  case DocumentMode::Dot:
    diagramDoc->clear();
    diagramToolbar->setActionsEnabled(false);
    graphvizRenderer->renderToSvgAsync(activeDocument->toPlainText());
    break;

  case DocumentMode::PlantUml:
    diagramDoc->clear();
    diagramToolbar->setActionsEnabled(false);
    plantUmlRenderer->renderToSvgAsync(activeDocument->toPlainText());
    break;

  case DocumentMode::Mermaid:
    diagramDoc->clear();
    diagramToolbar->setActionsEnabled(false);
    mermaidRenderer->renderToSvgAsync(activeDocument->toPlainText());
    break;

  default:
    break;
  }
}
void TextWidget::onElementClicked(const QString &id, const QString &name,
                                  const QPoint &globalPos) {
  Q_UNUSED(name);
  Q_UNUSED(globalPos);

  const auto info = diagramDoc->infoForId(id);
  if (info.nodeKind == DiagramDocument::NodeKind::Plain) return;
  if (info.referencePath.isEmpty()) return;

  if (info.nodeKind == DiagramDocument::NodeKind::Application) {
    QString spec = info.referencePath;
    if (activeDocument && !activeDocument->filePath().isEmpty())
      spec.replace("%f", activeDocument->filePath());
    const QStringList parts = QProcess::splitCommand(spec);
    if (parts.isEmpty()) return;
    if (!QProcess::startDetached(parts.first(), parts.mid(1)))
      emit statusMessage(tr("Could not launch: %1").arg(parts.first()), 4000);
    return;
  }

  if (info.nodeKind == DiagramDocument::NodeKind::External) {
    QDesktopServices::openUrl(QUrl(info.referencePath));
    return;
  }

  if (info.nodeKind == DiagramDocument::NodeKind::Reference) {
    const QString path = info.referencePath;
    const QString ext = QFileInfo(path).suffix().toLower();
    static const QSet<QString> editorExts = {
        "md",   "markdown", "txt",  "dot", "gv",
        "puml", "plantuml", "mmd",  "mermaid",
        "html", "htm"};
    if (editorExts.contains(ext) || ext.isEmpty()) {
      emit openDocumentRequested(path);
    } else {
      QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    }
  }
}

void TextWidget::setProjectRoot(const QString &root) {
  diagramDoc->setProjectRoot(root);
}

void TextWidget::setActiveDocument(TextDocument *document) {
  if (activeDocument == document) {
    syncPreview();
    return;
  }

  disconnectActiveDocument();

  activeDocument = document;

  if (!activeDocument) {
    clearPreview();
    return;
  }

  textEdit->setDocument(activeDocument);
  textEdit->setDocumentMode(activeDocument->type());

  if (previewController) {
    previewController->setEditor(textEdit);
  }

  connect(activeDocument, &QTextDocument::contentsChanged, this,
          &TextWidget::syncPreview);

  syncPreview();
}

void TextWidget::syncPreview() {
  if (!activeDocument) {
    clearPreview();
    return;
  }

  if (textBrowser->document() != previewDocument) {
    textBrowser->setDocument(previewDocument);
  }

  switch (activeDocument->type()) {
  case DocumentMode::Markdown:
    viewStack->setCurrentWidget(textBrowser);
    previewDocument->setMarkdown(activeDocument->toPlainText(),
                                 QTextDocument::MarkdownDialectGitHub);
    break;

  case DocumentMode::Html:
    viewStack->setCurrentWidget(textBrowser);
    previewDocument->setHtml(activeDocument->toPlainText());
    break;

  case DocumentMode::PlainText:
    viewStack->setCurrentWidget(textBrowser);
    previewDocument->setPlainText(activeDocument->toPlainText());
    break;

  case DocumentMode::Dot:
    viewStack->setCurrentWidget(diagramPage);
    diagramDoc->clear();
    diagramToolbar->setActionsEnabled(false);
    graphvizRenderer->renderToSvgAsync(activeDocument->toPlainText());
    break;

  case DocumentMode::PlantUml:
    viewStack->setCurrentWidget(diagramPage);
    diagramDoc->clear();
    diagramToolbar->setActionsEnabled(false);
    plantUmlRenderer->renderToSvgAsync(activeDocument->toPlainText());
    break;

  case DocumentMode::Mermaid:
    viewStack->setCurrentWidget(diagramPage);
    diagramDoc->clear();
    diagramToolbar->setActionsEnabled(false);
    mermaidRenderer->renderToSvgAsync(activeDocument->toPlainText());
    break;
  }
}

void TextWidget::disconnectActiveDocument() {
  if (!activeDocument) return;
  disconnect(activeDocument, &QTextDocument::contentsChanged, this,
             &TextWidget::syncPreview);
}

void TextWidget::clearPreview() {
  if (textBrowser->document() != previewDocument) {
    textBrowser->setDocument(previewDocument);
  }
  previewDocument->clear();
  diagramDoc->clear();
  diagramToolbar->setActionsEnabled(false);
}

void TextWidget::findInEditor(const QString &text) {
  if (text.isEmpty() || !textEdit) return;

  setViewMode(false);

  QTextCursor c = textEdit->textCursor();
  c.movePosition(QTextCursor::Start);
  textEdit->setTextCursor(c);

  const QTextDocument::FindFlags cs = QTextDocument::FindCaseSensitively;
  if (textEdit->find(text, cs)) {
    textEdit->ensureCursorVisible();
    textEdit->setFocus();
    return;
  }

  c.movePosition(QTextCursor::Start);
  textEdit->setTextCursor(c);
  if (textEdit->find(text)) {
    textEdit->ensureCursorVisible();
    textEdit->setFocus();
    return;
  }

  emit statusMessage(tr("Not found in editor: %1").arg(text));
}

void TextWidget::showNodeContextMenu(const QString &id, const QString &name,
                                     const QPoint &globalPos) {
  QMenu menu;

  if (!name.isEmpty()) {
    menu.addAction(tr("Copy name"),
                   [name] { QApplication::clipboard()->setText(name); });
    if (activeDocument &&
        (activeDocument->type() == DocumentMode::Dot ||
         activeDocument->type() == DocumentMode::PlantUml ||
         activeDocument->type() == DocumentMode::Mermaid)) {
      menu.addAction(tr("Find in editor"),
                     [this, name] { findInEditor(name); });
    }
  }

  const auto info = diagramDoc->infoForId(id);
  if (info.nodeKind != DiagramDocument::NodeKind::Plain &&
      !info.referencePath.isEmpty()) {
    const QString label = (info.nodeKind == DiagramDocument::NodeKind::External)
                              ? tr("Open in browser: %1").arg(info.referencePath)
                              : tr("Open: %1").arg(info.referencePath);
    const QString target = info.referencePath;
    const auto kind = info.nodeKind;
    menu.addSeparator();
    menu.addAction(label, [this, target, kind] {
      if (kind == DiagramDocument::NodeKind::External) {
        QDesktopServices::openUrl(QUrl(target));
      } else {
        emit openDocumentRequested(target);
      }
    });
  }

  menu.addSeparator();
  menu.addAction(tr("Focus on this node"),
                 [this, id] { svgView->focusOnElement(id); });
  menu.addAction(tr("Reset zoom"), [this] { svgView->zoomReset(); });

  menu.exec(globalPos);
}