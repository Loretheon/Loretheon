#include "TextWidget.h"
#include "DiagramDocument.h"
#include "DiagramToolbar.h"
#include "DiagramView.h"

#include <QApplication>
#include <QClipboard>
#include <QDebug>
#include <QDesktopServices>
#include <QFileInfo>
#include <QMenu>
#include <QPalette>
#include <QProcess>
#include <QUrl>
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

  connect(diagramToolbar, &DiagramToolbar::zoomInRequested,    svgView, &DiagramView::zoomIn);
  connect(diagramToolbar, &DiagramToolbar::zoomOutRequested,   svgView, &DiagramView::zoomOut);
  connect(diagramToolbar, &DiagramToolbar::zoomResetRequested, svgView, &DiagramView::zoomReset);
  connect(diagramToolbar, &DiagramToolbar::fitRequested,       svgView, &DiagramView::zoomFit);
  connect(diagramToolbar, &DiagramToolbar::zoomToRequested,    svgView, &DiagramView::setZoom);
  connect(svgView, &DiagramView::zoomChanged, diagramToolbar, &DiagramToolbar::setZoom);

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
            qDebug().noquote() << styled.mid(0, 3000);
            diagramDoc->setSvg(styled);
            diagramToolbar->setActionsEnabled(true);
          });

  connect(graphvizRenderer, &GraphvizRenderer::renderFailed, this,
          [this](const QString &err) {
            qWarning() << "Graphviz:" << err;
            diagramDoc->clear();
            diagramToolbar->setActionsEnabled(false);
            emit statusMessage(tr("Graphviz: %1").arg(err), 5000);
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
          [this](const QString &id, const QString &name, const QPoint &globalPos) {
            showNodeContextMenu(id, name, globalPos);
          });

  diagramToolbar->setActionsEnabled(false);
  diagramToolbar->setZoom(1.0);
}

void TextWidget::onElementClicked(const QString &id,
                                  const QString &name,
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
    static const QSet<QString> editorExts = {"md", "markdown", "txt", "dot", "gv", "html", "htm"};
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

  setCurrentWidget(textEdit);

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

void TextWidget::showNodeContextMenu(const QString &id,
                                     const QString &name,
                                     const QPoint &globalPos) {
  QMenu menu;

  if (!name.isEmpty()) {
    menu.addAction(tr("Copy name"), [name] {
      QApplication::clipboard()->setText(name);
    });
    if (activeDocument && activeDocument->type() == DocumentMode::Dot) {
      menu.addAction(tr("Find in editor"), [this, name] {
        findInEditor(name);
      });
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
  menu.addAction(tr("Focus on this node"), [this, id] {
    svgView->focusOnElement(id);
  });
  menu.addAction(tr("Reset zoom"), [this] {
    svgView->zoomReset();
  });

  menu.exec(globalPos);
}