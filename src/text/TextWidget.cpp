#include "TextWidget.h"

#include <QTextDocument>

TextWidget::TextWidget(QWidget *parent)
    : QTabWidget(parent), textEdit(new TextEdit(this)),
      textBrowser(new TextBrowser(this)),
      previewDocument(new QTextDocument(this)) {
  addTab(textEdit, tr("Edit"));
  addTab(textBrowser, tr("View"));

  textBrowser->setDocument(previewDocument);
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
    previewDocument->setMarkdown(activeDocument->toPlainText(),
                                 QTextDocument::MarkdownDialectGitHub);
    break;

  case DocumentMode::Html:
    previewDocument->setHtml(activeDocument->toPlainText());
    break;

  case DocumentMode::PlainText:
    previewDocument->setPlainText(activeDocument->toPlainText());
    break;
  }
}

void TextWidget::disconnectActiveDocument() {
  if (!activeDocument) {
    return;
  }

  disconnect(activeDocument, &QTextDocument::contentsChanged, this,
             &TextWidget::syncPreview);
}

void TextWidget::clearPreview() {
  if (textBrowser->document() != previewDocument) {
    textBrowser->setDocument(previewDocument);
  }

  previewDocument->clear();
}