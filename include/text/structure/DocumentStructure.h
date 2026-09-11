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