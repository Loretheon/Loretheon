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

  // Convenience accessor for the token layer. Returns the cached content
  // hash for a scope, or an empty string if the scope is unknown.
  QString scopeContentHash(const QString &scopeId) const;

private:
  QString path;

  DocumentMode docType = DocumentMode::PlainText;

  mutable DocumentStructure m_structure;

  mutable int m_structureRevision = -1;

  mutable TSParser *m_markdownParser = nullptr;

  mutable TSTree *m_markdownTree = nullptr;
};

#endif // EPISTEME_TEXTDOCUMENT_H