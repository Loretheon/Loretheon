#include "TextDocument.h"

#include "../structure/MarkdownStructureParser.h"
#include "../structure/PlainTextStructureParser.h"

#include <QPlainTextDocumentLayout>

namespace {

void resetMarkdownState(TSParser *&parser, TSTree *&tree) {
  MarkdownStructureParser::destroyParser(parser);
  MarkdownStructureParser::destroyTree(tree);
}

DocumentStructure buildStructure(DocumentMode mode, const QString &text,
                                 TSParser *&markdownParser,
                                 TSTree *&markdownTree,
                                 const QString &previousText) {
  switch (mode) {
  case DocumentMode::Markdown: {
    MarkdownStructureParser parser;

    return parser.parse(text, markdownParser, markdownTree, previousText);
  }

  case DocumentMode::PlainText: {
    PlainTextStructureParser parser;
    return parser.parse(text);
  }

  case DocumentMode::Html: {
    PlainTextStructureParser parser;
    return parser.parse(text);
  }

  case DocumentMode::Dot: {
    PlainTextStructureParser parser;
    return parser.parse(text);
  }
  }

  return {};
}

} // namespace

TextDocument::TextDocument(QObject *parent) : QTextDocument(parent) {
  setDocumentLayout(new QPlainTextDocumentLayout(this));
}

TextDocument::~TextDocument() {
  resetMarkdownState(m_markdownParser, m_markdownTree);
}

QString TextDocument::filePath() const { return path; }

void TextDocument::setFilePath(const QString &newPath) {
  if (path == newPath) {
    return;
  }

  path = newPath;
}

DocumentMode TextDocument::type() const { return docType; }

void TextDocument::setType(DocumentMode newType) {
  if (docType == newType) {
    return;
  }

  docType = newType;
  m_structureRevision = -1;

  resetMarkdownState(m_markdownParser, m_markdownTree);
}

const DocumentStructure &TextDocument::structure() const { return m_structure; }

void TextDocument::rebuildStructure() const {
  const int currentRevision = revision();

  if (m_structureRevision == currentRevision) {
    return;
  }

  const QString currentText = toPlainText();
  const QString previousText = m_structure.text();

  const DocumentStructure rebuilt = buildStructure(
      docType, currentText, m_markdownParser, m_markdownTree, previousText);

  m_structure = rebuilt;
  m_structureRevision = currentRevision;
}