#pragma once

#include "DocumentStructure.h"

#include <QString>

struct TSParser;
struct TSTree;
struct TSNode;

class MarkdownStructureParser {
public:
  DocumentStructure parse(const QString &text) const;

  DocumentStructure parse(const QString &text, TSParser *&parser,
                          TSTree *&tree, const QString &previousText) const;

  static void destroyParser(TSParser *&parser);

  static void destroyTree(TSTree *&tree);

private:
  static DocumentNode makeNode(const TSNode &node, const QString &text,
                               const QString &parentChain);

  static void appendChildren(const TSNode &node, DocumentNode &parent,
                             const QString &text, const QString &parentChain);

  static QString slugify(const QString &text);

  static QString makeStableId(const QString &parentChain,
                              const QString &headingLine);

  static QString contentHashFor(const QString &text, int start, int end);
};