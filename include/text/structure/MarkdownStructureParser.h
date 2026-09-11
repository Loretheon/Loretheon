#pragma once

#include "DocumentStructure.h"

struct TSParser;
struct TSTree;
struct TSNode;

class MarkdownStructureParser {
public:
  DocumentStructure parse(const QString &text) const;

  DocumentStructure parse(const QString &text, TSParser *&parser, TSTree *&tree,
                          const QString &previousText) const;

  static void destroyParser(TSParser *&parser);

  static void destroyTree(TSTree *&tree);

private:
  static DocumentNode makeNode(const TSNode &node, const QString &text);

  static void appendChildren(const TSNode &node, DocumentNode &parent,
                             const QString &text);
};