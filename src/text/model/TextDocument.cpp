#include "TextDocument.h"

TextDocument::TextDocument(QObject *parent) : QTextDocument(parent)
{
}

QString TextDocument::filePath() const
{
  return path;
}

void TextDocument::setFilePath(const QString &newPath)
{
  path = newPath;
}

TextDocument::Type TextDocument::type() const
{
  return docType;
}

void TextDocument::setType(Type newType)
{
  docType = newType;
}