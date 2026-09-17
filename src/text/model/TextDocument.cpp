#include "TextDocument.h"

#include "../structure/DotStructureParser.h"
#include "../structure/MarkdownStructureParser.h"
#include "../structure/MermaidStructureParser.h"
#include "../structure/PlainTextStructureParser.h"
#include "../structure/PlantUmlStructureParser.h"

#include <QPlainTextDocumentLayout>

namespace {

void resetMarkdownState(TSParser *&parser, TSTree *&tree) {
  MarkdownStructureParser::destroyParser(parser);
  MarkdownStructureParser::destroyTree(tree);
}

void resetDotState(TSParser *&parser, TSTree *&tree) {
  DotStructureParser::destroyParser(parser);
  DotStructureParser::destroyTree(tree);
}

void resetMermaidState(TSParser *&parser, TSTree *&tree) {
  MermaidStructureParser::destroyParser(parser);
  MermaidStructureParser::destroyTree(tree);
}

DocumentStructure buildStructure(DocumentMode mode, const QString &text,
                                 TSParser *&markdownParser,
                                 TSTree *&markdownTree,
                                 TSParser *&dotParser,
                                 TSTree *&dotTree,
                                 TSParser *&mermaidParser,
                                 TSTree *&mermaidTree,
                                 const QString &previousText) {
  switch (mode) {
  case DocumentMode::Markdown: {
    MarkdownStructureParser parser;

    return parser.parse(text, markdownParser, markdownTree, previousText);
  }

  case DocumentMode::Dot: {
    DotStructureParser parser;

    return parser.parse(text, dotParser, dotTree, previousText);
  }

  case DocumentMode::Mermaid: {
    MermaidStructureParser parser;

    return parser.parse(text, mermaidParser, mermaidTree, previousText);
  }

  case DocumentMode::PlantUml: {
    PlantUmlStructureParser parser;
    return parser.parse(text);
  }

  case DocumentMode::PlainText:
  case DocumentMode::Html: {
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
  resetDotState(m_dotParser, m_dotTree);
  resetMermaidState(m_mermaidParser, m_mermaidTree);
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
  resetDotState(m_dotParser, m_dotTree);
  resetMermaidState(m_mermaidParser, m_mermaidTree);
}

const DocumentStructure &TextDocument::structure() const { return m_structure; }

QString TextDocument::scopeContentHash(const QString &scopeId) const {
  return m_structure.contentHashFor(scopeId);
}

void TextDocument::rebuildStructure() const {
  const int currentRevision = revision();

  if (m_structureRevision == currentRevision) {
    return;
  }

  const QString currentText = toPlainText();
  const QString previousText = m_structure.text();

  const DocumentStructure rebuilt =
      buildStructure(docType, currentText, m_markdownParser, m_markdownTree,
                     m_dotParser, m_dotTree, m_mermaidParser, m_mermaidTree,
                     previousText);

  m_structure = rebuilt;
  m_structureRevision = currentRevision;
}