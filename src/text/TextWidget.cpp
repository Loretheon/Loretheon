// TextWidget.cpp
#include "TextWidget.h"
#include "DiagramDocument.h"
#include "DiagramToolbar.h"
#include "DiagramView.h"

#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QMenu>
#include <QPalette>
#include <QStatusBar>
#include <QVBoxLayout>

TextWidget::TextWidget(QWidget *parent)
    : QTabWidget(parent),
      textEdit(new TextEdit(this)),
      textBrowser(new TextBrowser(this)),
      viewStack(new QStackedWidget(this)),
      diagramPage(new QWidget(this)),
      svgView(new DiagramView(this)),
      diagramToolbar(new DiagramToolbar(this)),
      diagramDoc(new DiagramDocument(this)),
      previewDocument(new QTextDocument(this)),
      graphvizRenderer(new GraphvizRenderer(this)) {
  addTab(textEdit, tr("Edit"));

  viewStack->addWidget(textBrowser);
  viewStack->addWidget(diagramPage);
  addTab(viewStack, tr("View"));

  auto *pageLay = new QVBoxLayout(diagramPage);
  pageLay->setContentsMargins(0, 0, 0, 0);
  pageLay->setSpacing(0);
  pageLay->addWidget(diagramToolbar);
  pageLay->addWidget(svgView, 1);

  textBrowser->setDocument(previewDocument);
  svgView->setDocument(diagramDoc);

  connect(diagramToolbar, &DiagramToolbar::zoomInRequested,
          svgView, &DiagramView::zoomIn);
  connect(diagramToolbar, &DiagramToolbar::zoomOutRequested,
          svgView, &DiagramView::zoomOut);
  connect(diagramToolbar, &DiagramToolbar::zoomResetRequested,
          svgView, &DiagramView::zoomReset);
  connect(diagramToolbar, &DiagramToolbar::fitRequested,
          svgView, &DiagramView::zoomFit);
  connect(diagramToolbar, &DiagramToolbar::zoomToRequested,
          svgView, &DiagramView::setZoom);
  connect(svgView, &DiagramView::zoomChanged,
          diagramToolbar, &DiagramToolbar::setZoom);

  connect(graphvizRenderer, &GraphvizRenderer::svgReady, this,
          [this](const QString &raw) {
            if (raw.isEmpty()) {
              diagramDoc->clear();
              diagramToolbar->setActionsEnabled(false);
              return;
            }
            const QPalette pal = palette();
            const QString styled = GraphvizRenderer::optimizeGraphvizSvg(
                raw,
                pal.color(QPalette::Window).name(),
                pal.color(QPalette::WindowText).name());
            diagramDoc->setSvg(styled);
            diagramToolbar->setActionsEnabled(true);
          });

  connect(graphvizRenderer, &GraphvizRenderer::renderFailed, this,
          [this](const QString &err) {
            qWarning() << "Graphviz:" << err;
            diagramDoc->clear();
            diagramToolbar->setActionsEnabled(false);
          });

  connect(svgView, &DiagramView::elementClicked, this,
          [this](const QString &, const QString &name, const QPoint &) {
            if (!name.isEmpty())
              findInEditor(name);
          });

  connect(svgView, &DiagramView::elementRightClicked, this,
          [this](const QString &id, const QString &name, const QPoint &globalPos) {
            QMenu menu;
            if (!name.isEmpty()) {
              menu.addAction(tr("Copy name"), [name] {
                QApplication::clipboard()->setText(name);
              });
              menu.addAction(tr("Find in editor"), [this, name] {
                findInEditor(name);
              });
              menu.addSeparator();
            }
            menu.addAction(tr("Focus on this node"), [this, id] {
              svgView->focusOnElement(id);
            });
            menu.addAction(tr("Reset zoom"), [this] {
              svgView->zoomReset();
            });
            menu.exec(globalPos);
          });

  diagramToolbar->setActionsEnabled(false);
  diagramToolbar->setZoom(1.0);
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
  QTextCursor c = textEdit->textCursor();
  c.movePosition(QTextCursor::Start);
  textEdit->setTextCursor(c);

  QTextDocument::FindFlags flags = QTextDocument::FindCaseSensitively;
  if (textEdit->find(text, flags) || textEdit->find(text)) {
    textEdit->ensureCursorVisible();
  }
}