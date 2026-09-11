#include "MarkdownStructureParser.h"

#include <tree-sitter/tree-sitter-markdown.h>
#include <tree_sitter/api.h>

#include <QByteArray>
#include <QString>

namespace {

QString nodeId(const TSNode &node) {
  if (ts_node_is_null(node)) {
    return {};
  }

  const auto rawId = reinterpret_cast<quintptr>(node.id);

  return QStringLiteral("ts:%1").arg(static_cast<qulonglong>(rawId), 0, 16);
}

bool ensureParser(TSParser *&parser) {
  if (parser) {
    return true;
  }

  parser = ts_parser_new();

  if (!parser) {
    return false;
  }

  if (ts_parser_set_language(parser, tree_sitter_markdown())) {
    return true;
  }

  ts_parser_delete(parser);
  parser = nullptr;

  return false;
}

TSPoint pointAtByte(const QByteArray &text, int byteOffset) {
  TSPoint point{0, 0};

  const int offset = qBound(0, byteOffset, text.size());

  for (int index = 0; index < offset; ++index) {
    if (text.at(index) == '\n') {
      ++point.row;
      point.column = 0;
    } else {
      ++point.column;
    }
  }

  return point;
}

TSInputEdit computeEdit(const QByteArray &oldText, const QByteArray &newText) {
  const int oldSize = oldText.size();
  const int newSize = newText.size();

  int prefix = 0;

  while (prefix < oldSize && prefix < newSize &&
         oldText.at(prefix) == newText.at(prefix)) {
    ++prefix;
  }

  int oldSuffix = oldSize;
  int newSuffix = newSize;

  while (oldSuffix > prefix && newSuffix > prefix &&
         oldText.at(oldSuffix - 1) == newText.at(newSuffix - 1)) {
    --oldSuffix;
    --newSuffix;
  }

  TSInputEdit edit{};

  edit.start_byte = static_cast<uint32_t>(prefix);

  edit.old_end_byte = static_cast<uint32_t>(oldSuffix);

  edit.new_end_byte = static_cast<uint32_t>(newSuffix);

  edit.start_point = pointAtByte(oldText, prefix);

  edit.old_end_point = pointAtByte(oldText, oldSuffix);

  edit.new_end_point = pointAtByte(newText, newSuffix);

  return edit;
}

int utf16Offset(const QByteArray &utf8, uint32_t byteOffset) {
  const int offset = qBound(0, static_cast<int>(byteOffset), utf8.size());

  if (offset == 0) {
    return 0;
  }

  return QString::fromUtf8(utf8.constData(), offset).size();
}

} // namespace

DocumentStructure MarkdownStructureParser::parse(const QString &text) const {
  TSParser *parser = ts_parser_new();

  if (!parser) {
    return {};
  }

  if (!ts_parser_set_language(parser, tree_sitter_markdown())) {
    ts_parser_delete(parser);
    return {};
  }

  const QByteArray utf8 = text.toUtf8();

  TSTree *tree = ts_parser_parse_string(parser, nullptr, utf8.constData(),
                                        static_cast<uint32_t>(utf8.size()));

  if (!tree) {
    ts_parser_delete(parser);
    return {};
  }

  DocumentStructure structure;
  structure.setText(text);

  const TSNode rootNode = ts_tree_root_node(tree);

  DocumentNode root = makeNode(rootNode, text);

  appendChildren(rootNode, root, text);

  structure.setRoot(std::move(root));

  ts_tree_delete(tree);
  ts_parser_delete(parser);

  return structure;
}

DocumentStructure
MarkdownStructureParser::parse(const QString &text, TSParser *&parser,
                               TSTree *&tree,
                               const QString &previousText) const {
  if (!ensureParser(parser)) {
    return {};
  }

  const QByteArray newUtf8 = text.toUtf8();

  if (tree && previousText != text) {
    const QByteArray oldUtf8 = previousText.toUtf8();

    const TSInputEdit edit = computeEdit(oldUtf8, newUtf8);

    ts_tree_edit(tree, &edit);
  }

  TSTree *newTree = ts_parser_parse_string(
      parser, tree, newUtf8.constData(), static_cast<uint32_t>(newUtf8.size()));

  if (!newTree) {
    return {};
  }

  if (tree && tree != newTree) {
    ts_tree_delete(tree);
  }

  tree = newTree;

  DocumentStructure structure;
  structure.setText(text);

  const TSNode rootNode = ts_tree_root_node(tree);

  DocumentNode root = makeNode(rootNode, text);

  appendChildren(rootNode, root, text);

  structure.setRoot(std::move(root));

  return structure;
}

void MarkdownStructureParser::destroyParser(TSParser *&parser) {
  if (!parser) {
    return;
  }

  ts_parser_delete(parser);
  parser = nullptr;
}

void MarkdownStructureParser::destroyTree(TSTree *&tree) {
  if (!tree) {
    return;
  }

  ts_tree_delete(tree);
  tree = nullptr;
}

DocumentNode MarkdownStructureParser::makeNode(const TSNode &node,
                                               const QString &text) {
  DocumentNode result;

  if (ts_node_is_null(node)) {
    return result;
  }

  const QByteArray utf8 = text.toUtf8();

  result.id = nodeId(node);
  result.type = QString::fromUtf8(ts_node_type(node));

  result.start = utf16Offset(utf8, ts_node_start_byte(node));

  result.end = utf16Offset(utf8, ts_node_end_byte(node));

  return result;
}

void MarkdownStructureParser::appendChildren(const TSNode &node,
                                             DocumentNode &parent,
                                             const QString &text) {
  const uint32_t childCount = ts_node_named_child_count(node);

  for (uint32_t index = 0; index < childCount; ++index) {
    const TSNode child = ts_node_named_child(node, index);

    if (ts_node_is_null(child)) {
      continue;
    }

    DocumentNode childNode = makeNode(child, text);

    appendChildren(child, childNode, text);

    parent.children.append(std::move(childNode));
  }
}