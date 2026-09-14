#include "MarkdownStructureParser.h"

#include <tree-sitter/tree-sitter-markdown.h>
#include <tree_sitter/api.h>

#include <QByteArray>
#include <QCryptographicHash>
#include <QRegularExpression>
#include <QString>

namespace {

constexpr int StableHashLength = 6;
constexpr int ContentHashLength = 12;

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

QString firstHeadingLine(const QString &text, int start, int end) {
  const int safeStart = qBound(0, start, text.size());
  const int safeEnd = qBound(safeStart, end, text.size());

  if (safeEnd <= safeStart) {
    return {};
  }

  int cursor = safeStart;

  while (cursor < safeEnd) {
    int lineEnd = text.indexOf(QChar('\n'), cursor);

    if (lineEnd < 0 || lineEnd > safeEnd) {
      lineEnd = safeEnd;
    }

    const QString line = text.mid(cursor, lineEnd - cursor).trimmed();

    if (line.startsWith(QChar('#'))) {
      return line;
    }

    cursor = lineEnd + 1;
  }

  return {};
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

  DocumentNode root = makeNode(rootNode, text, QString());

  appendChildren(rootNode, root, text, QString());

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

  DocumentNode root = makeNode(rootNode, text, QString());

  appendChildren(rootNode, root, text, QString());

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

QString MarkdownStructureParser::slugify(const QString &text) {
  QString result = text.trimmed();

  while (result.startsWith(QChar('#'))) {
    result.remove(0, 1);
  }

  result = result.trimmed();

  static const QRegularExpression inlineMarkup(QStringLiteral("[`*_~]"));

  result.remove(inlineMarkup);

  result = result.toLower();

  static const QRegularExpression nonAlnum(QStringLiteral("[^a-z0-9]+"));

  result.replace(nonAlnum, QStringLiteral("-"));

  while (result.startsWith(QChar('-'))) {
    result.remove(0, 1);
  }

  while (result.endsWith(QChar('-'))) {
    result.chop(1);
  }

  if (result.isEmpty()) {
    result = QStringLiteral("section");
  }

  return result;
}

QString MarkdownStructureParser::makeStableId(const QString &parentChain,
                                              const QString &headingLine) {
  const QString slug = slugify(headingLine);

  const QByteArray hashInput =
      (parentChain + QStringLiteral("|") + headingLine).toUtf8();

  const QByteArray digest =
      QCryptographicHash::hash(hashInput, QCryptographicHash::Sha1);

  const QString hashHex =
      QString::fromLatin1(digest.toHex()).left(StableHashLength);

  return QStringLiteral("md:section:%1#%2").arg(slug, hashHex);
}

QString MarkdownStructureParser::contentHashFor(const QString &text, int start,
                                                int end) {
  const int safeStart = qBound(0, start, text.size());
  const int safeEnd = qBound(safeStart, end, text.size());

  if (safeEnd <= safeStart) {
    return {};
  }

  const QByteArray utf8 = text.mid(safeStart, safeEnd - safeStart).toUtf8();

  const QByteArray digest =
      QCryptographicHash::hash(utf8, QCryptographicHash::Sha1);

  return QString::fromLatin1(digest.toHex()).left(ContentHashLength);
}

DocumentNode MarkdownStructureParser::makeNode(const TSNode &node,
                                               const QString &text,
                                               const QString &parentChain) {
  DocumentNode result;

  if (ts_node_is_null(node)) {
    return result;
  }

  const QByteArray utf8 = text.toUtf8();

  result.type = QString::fromUtf8(ts_node_type(node));

  result.start = utf16Offset(utf8, ts_node_start_byte(node));

  result.end = utf16Offset(utf8, ts_node_end_byte(node));

  // The root node always gets a stable identifier so callers have a
  // well-known way to reference the whole document. Section nodes get
  // content-derived IDs. All other nodes stay anonymous.
  if (ts_node_is_null(ts_node_parent(node))) {
    result.id = QStringLiteral("document");
  } else if (result.type == QStringLiteral("section")) {
    const QString headingLine =
        firstHeadingLine(text, result.start, result.end);

    result.id = makeStableId(parentChain, headingLine);
  }

  if (result.end > result.start) {
    result.contentHash = contentHashFor(text, result.start, result.end);
  }

  return result;
}

void MarkdownStructureParser::appendChildren(const TSNode &node,
                                             DocumentNode &parent,
                                             const QString &text,
                                             const QString &parentChain) {
  const uint32_t childCount = ts_node_named_child_count(node);

  for (uint32_t index = 0; index < childCount; ++index) {
    const TSNode child = ts_node_named_child(node, index);

    if (ts_node_is_null(child)) {
      continue;
    }

    QString childChain = parentChain;

    DocumentNode childNode = makeNode(child, text, parentChain);

    if (childNode.type == QStringLiteral("section")) {
      const QString slug =
          slugify(firstHeadingLine(text, childNode.start, childNode.end));

      childChain = parentChain.isEmpty()
                       ? slug
                       : parentChain + QStringLiteral("/") + slug;
    }

    appendChildren(child, childNode, text, childChain);

    parent.children.append(std::move(childNode));
  }
}