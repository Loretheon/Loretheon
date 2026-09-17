#include "DotStructureParser.h"

#include <tree_sitter/api.h>

#include <QByteArray>
#include <QCryptographicHash>
#include <QRegularExpression>
#include <QString>

extern "C" const TSLanguage *tree_sitter_dot(void);

namespace {

constexpr int StableHashLength = 6;

bool ensureParser(TSParser *&parser) {
  if (parser) {
    return true;
  }

  parser = ts_parser_new();

  if (!parser) {
    return false;
  }

  if (ts_parser_set_language(parser, tree_sitter_dot())) {
    return true;
  }

  ts_parser_delete(parser);
  parser = nullptr;

  return false;
}

int utf16Offset(const QByteArray &utf8, uint32_t byteOffset) {
  const int offset = qBound(0, static_cast<int>(byteOffset), utf8.size());

  if (offset == 0) {
    return 0;
  }

  return QString::fromUtf8(utf8.constData(), offset).size();
}

QString firstIdentifierLine(const QString &text, int start, int end) {
  const int safeStart = qBound(0, start, text.size());
  const int safeEnd = qBound(safeStart, end, text.size());

  if (safeEnd <= safeStart) {
    return {};
  }

  return text.mid(safeStart, safeEnd - safeStart).trimmed().left(120);
}

} // namespace

DocumentStructure DotStructureParser::parse(const QString &text) const {
  TSParser *parser = ts_parser_new();

  if (!parser) {
    return {};
  }

  if (!ts_parser_set_language(parser, tree_sitter_dot())) {
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

DocumentStructure DotStructureParser::parse(const QString &text,
                                            TSParser *&parser, TSTree *&tree,
                                            const QString &previousText) const {
  Q_UNUSED(previousText);

  if (!ensureParser(parser)) {
    return {};
  }

  const QByteArray utf8 = text.toUtf8();

  TSTree *newTree = ts_parser_parse_string(
      parser, tree, utf8.constData(), static_cast<uint32_t>(utf8.size()));

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

void DotStructureParser::destroyParser(TSParser *&parser) {
  if (!parser) {
    return;
  }

  ts_parser_delete(parser);
  parser = nullptr;
}

void DotStructureParser::destroyTree(TSTree *&tree) {
  if (!tree) {
    return;
  }

  ts_tree_delete(tree);
  tree = nullptr;
}

QString DotStructureParser::slugify(const QString &text) {
  QString result = text.trimmed();

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
    result = QStringLiteral("node");
  }

  return result;
}

QString DotStructureParser::makeStableId(const QString &kind,
                                         const QString &slug,
                                         const QString &parentChain) {
  const QByteArray hashInput =
      (parentChain + QStringLiteral("|") + kind + QStringLiteral("|") + slug)
          .toUtf8();

  const QByteArray digest =
      QCryptographicHash::hash(hashInput, QCryptographicHash::Sha1);

  const QString hashHex =
      QString::fromLatin1(digest.toHex()).left(StableHashLength);

  return QStringLiteral("dot:%1:%2#%3").arg(kind, slug, hashHex);
}

DocumentNode DotStructureParser::makeNode(const TSNode &node,
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

  const QString type = result.type;

  const bool isRoot = ts_node_is_null(ts_node_parent(node));

  if (isRoot) {
    result.id = QStringLiteral("document");
  } else if (type == QStringLiteral("node_id") ||
             type == QStringLiteral("node_stmt")) {
    const QString line = firstIdentifierLine(text, result.start, result.end);

    if (!line.isEmpty()) {
      result.id = makeStableId(QStringLiteral("node"), slugify(line),
                               parentChain);
    }
  } else if (type == QStringLiteral("edge_stmt")) {
    const QString line = firstIdentifierLine(text, result.start, result.end);

    if (!line.isEmpty()) {
      result.id = makeStableId(QStringLiteral("edge"), slugify(line),
                               parentChain);
    }
  } else if (type == QStringLiteral("subgraph")) {
    const QString line = firstIdentifierLine(text, result.start, result.end);

    if (!line.isEmpty()) {
      result.id = makeStableId(QStringLiteral("subgraph"), slugify(line),
                               parentChain);
    }
  }

  if (result.end > result.start) {
    const QByteArray nodeBytes =
        text.mid(result.start, result.end - result.start).toUtf8();

    const QByteArray digest =
        QCryptographicHash::hash(nodeBytes, QCryptographicHash::Sha1);

    result.contentHash = QString::fromLatin1(digest.toHex()).left(12);
  }

  return result;
}

void DotStructureParser::appendChildren(const TSNode &node,
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

    if (!childNode.id.isEmpty() && childNode.id != QStringLiteral("document")) {
      childChain = parentChain.isEmpty()
                       ? childNode.id
                       : parentChain + QStringLiteral("/") + childNode.id;
    }

    appendChildren(child, childNode, text, childChain);

    parent.children.append(std::move(childNode));
  }
}