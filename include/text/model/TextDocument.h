#ifndef EPISTEME_TEXTDOCUMENT_H
#define EPISTEME_TEXTDOCUMENT_H

#include "../structure/DocumentStructure.h"
#include "DocumentMode.h"

#include <QTextDocument>

struct TSParser;
struct TSTree;

class TextDocument : public QTextDocument {
  Q_OBJECT

public:
  explicit TextDocument(QObject *parent = nullptr);

  ~TextDocument() override;

  QString filePath() const;

  void setFilePath(const QString &path);

  DocumentMode type() const;

  void setType(DocumentMode type);

  const DocumentStructure &structure() const;

  void rebuildStructure() const;

private:
  QString path;

  DocumentMode docType = DocumentMode::PlainText;

  mutable DocumentStructure m_structure;

  mutable int m_structureRevision = -1;

  /*
   * Markdown parser state is kept with the document so
   * Tree-sitter can incrementally reparse after edits.
   */
  mutable TSParser *m_markdownParser = nullptr;

  mutable TSTree *m_markdownTree = nullptr;
};

#endif // EPISTEME_TEXTDOCUMENT_H