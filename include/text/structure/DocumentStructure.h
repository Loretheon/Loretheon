#pragma once

#include "DocumentNode.h"

#include <QString>
#include <QStringList>

class DocumentStructure {
public:
  void clear();

  void setText(const QString &text);

  void setRoot(DocumentNode root);

  const QString &text() const;

  const DocumentNode &root() const;

  const DocumentNode *find(const QString &id) const;

  QStringList scopeIds() const;

  // Returns the cached contentHash for a scope, or an empty string if the
  // scope is not found or has no hash.
  QString contentHashFor(const QString &scopeId) const;

  // Returns the character range covering just the heading line of a
  // Markdown section (i.e. the first line beginning with '#'). For
  // non-section scopes or headings that cannot be located, returns an
  // invalid range (start == -1). The returned range is half-open.
  bool headingRange(const QString &scopeId, int &start, int &end) const;

  QString sectionIndexForModel(int maxPreviewCharacters = 80) const;

  QString indexForModel(int maxPreviewCharacters = 80) const;

private:
  const DocumentNode *findRecursive(const DocumentNode &node,
                                    const QString &id) const;

  void appendScopeIdsRecursive(const DocumentNode &node,
                               QStringList &ids) const;

  void appendSectionIndexRecursive(const DocumentNode &node, QString &output,
                                   int depth, int maxPreviewCharacters) const;

  void appendIndexRecursive(const DocumentNode &node, QString &output,
                            int depth, int maxPreviewCharacters) const;

  QString m_text;

  DocumentNode m_root;
};