#include "TextDocument.h"

#include "TextEdit.h"

TextDocument::TextDocument(QObject *parent) : QTextDocument(parent) {
  setDocumentLayout(new QPlainTextDocumentLayout(this));
}

QString TextDocument::filePath() const { return path; }

void TextDocument::setFilePath(const QString &newPath) { path = newPath; }
DocumentMode TextDocument::type() const { return docType; }

void TextDocument::setType(DocumentMode newType) { docType = newType; }