#include "TextWidget.h"

TextWidget::TextWidget(QWidget *parent) : QTabWidget(parent) {
  textEdit = new TextEdit(this);
  textBrowser = new TextBrowser(this);

  addTab(textEdit, tr("Edit"));
  addTab(textBrowser, tr("View"));

  previewDocument = new QTextDocument(this);
  textBrowser->setDocument(previewDocument);
}

void TextWidget::setActiveDocument(TextDocument *newDocument) {
  qDebug() << "setActiveDocument called";

  if (activeDocument) {
    qDebug() << "Disconnecting previous document";
    disconnect(activeDocument, &QTextDocument::contentsChanged, this,
               &TextWidget::syncPreview);
  }

  activeDocument = newDocument;

  if (!activeDocument) {
    qDebug() << "No active document";
    return;
  }

  qDebug() << "Document pointer:" << activeDocument;
  qDebug() << "Document type:" << static_cast<int>(activeDocument->type());
  qDebug() << "Plain text length:" << activeDocument->toPlainText().length();
  qDebug() << "Plain text preview:" << activeDocument->toPlainText().left(100);

  textEdit->setDocument(activeDocument);

  textEdit->setDocumentMode(activeDocument->type());

  if (activeDocument->type() == DocumentMode::Markdown) {
    qDebug() << "Markdown document";

    connect(activeDocument, &QTextDocument::contentsChanged, this,
            &TextWidget::syncPreview);

    syncPreview();

    qDebug() << "Preview document length after markdown:"
             << previewDocument->toPlainText().length();
  } else {
    qDebug() << "Plain text document";

    previewDocument->setPlainText(activeDocument->toPlainText());

    qDebug() << "Preview document length after plain text:"
             << previewDocument->toPlainText().length();

    textBrowser->setDocument(previewDocument);

    qDebug() << "Text browser document:" << textBrowser->document();
  }
}

void TextWidget::syncPreview() {
  if (!activeDocument || activeDocument->type() != DocumentMode::Markdown) {
    return;
  }

  if (textBrowser->document() != previewDocument) {
    textBrowser->setDocument(previewDocument);
  }

  previewDocument->setMarkdown(activeDocument->toPlainText(),
                               QTextDocument::MarkdownDialectGitHub);
}