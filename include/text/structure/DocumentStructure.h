#pragma once

#include "DocumentNode.h"

#include <QString>

class DocumentStructure {
public:
  void clear();

  void setText(const QString &text);
  void setRoot(DocumentNode root);

  const QString &text() const;
  const DocumentNode &root() const;

  const DocumentNode *find(const QString &id) const;

  QString indexForModel(int maxPreviewCharacters = 80) const;

private:
  const DocumentNode *findRecursive(const DocumentNode &node,
                                    const QString &id) const;

  void appendIndexRecursive(const DocumentNode &node, QString &output,
                            int depth, int maxPreviewCharacters) const;

  QString m_text;
  DocumentNode m_root;
};