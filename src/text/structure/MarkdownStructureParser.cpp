#include "MarkdownStructureParser.h"

#include <tree_sitter/api.h>
#include <tree-sitter/tree-sitter-markdown.h>
#include <QByteArray>
#include <QDebug>
#include <QString>

namespace {

QString nodeId(
    const TSNode &node) {
    if (ts_node_is_null(node)) {
        return {};
    }

    const quintptr raw =
        reinterpret_cast<quintptr>(
            node.id);

    return QStringLiteral("ts:%1")
        .arg(
            static_cast<qulonglong>(raw),
            0,
            16);
}

TSPoint pointAtByte(
    const QByteArray &text,
    int byteOffset) {
    TSPoint point{
        0,
        0
    };

    const int safeOffset =
        qBound(
            0,
            byteOffset,
            text.size());

    for (int i = 0;
         i < safeOffset;
         ++i) {
        if (text.at(i) == '\n') {
            ++point.row;
            point.column = 0;
        } else {
            ++point.column;
        }
    }

    return point;
}

TSInputEdit computeEdit(
    const QByteArray &oldText,
    const QByteArray &newText) {
    int prefix = 0;

    const int oldSize =
        oldText.size();

    const int newSize =
        newText.size();

    while (prefix < oldSize &&
           prefix < newSize &&
           oldText.at(prefix) ==
               newText.at(prefix)) {
        ++prefix;
    }

    int oldSuffix =
        oldSize;

    int newSuffix =
        newSize;

    while (oldSuffix > prefix &&
           newSuffix > prefix &&
           oldText.at(oldSuffix - 1) ==
               newText.at(newSuffix - 1)) {
        --oldSuffix;
        --newSuffix;
    }

    TSInputEdit edit{};

    edit.start_byte =
        static_cast<uint32_t>(
            prefix);

    edit.old_end_byte =
        static_cast<uint32_t>(
            oldSuffix);

    edit.new_end_byte =
        static_cast<uint32_t>(
            newSuffix);

    edit.start_point =
        pointAtByte(
            oldText,
            prefix);

    edit.old_end_point =
        pointAtByte(
            oldText,
            oldSuffix);

    edit.new_end_point =
        pointAtByte(
            newText,
            newSuffix);

    return edit;
}

}

DocumentStructure
MarkdownStructureParser::parse(
    const QString &text) const {
    TSParser *parser =
        ts_parser_new();

    if (!parser) {
        qWarning()
            << "[STRUCTURE] Failed to create Tree-sitter parser.";

        return {};
    }

    if (!ts_parser_set_language(
            parser,
            tree_sitter_markdown())) {
        qWarning()
            << "[STRUCTURE] Failed to set Markdown language.";

        ts_parser_delete(
            parser);

        return {};
    }

    QByteArray utf8 =
        text.toUtf8();

    TSTree *tree =
        ts_parser_parse_string(
            parser,
            nullptr,
            utf8.constData(),
            static_cast<uint32_t>(
                utf8.size()));

    if (!tree) {
        qWarning()
            << "[STRUCTURE] Markdown parse failed.";

        ts_parser_delete(
            parser);

        return {};
    }

    DocumentStructure structure;

    structure.setText(
        text);

    DocumentNode root =
        makeNode(
            ts_tree_root_node(tree));

    appendChildren(
        ts_tree_root_node(tree),
        root);

    structure.setRoot(
        std::move(root));

    ts_tree_delete(
        tree);

    ts_parser_delete(
        parser);

    return structure;
}

DocumentStructure
MarkdownStructureParser::parse(
    const QString &text,
    TSParser *&parser,
    TSTree *&tree,
    const QString &previousText) const {
    if (!parser) {
        parser =
            ts_parser_new();

        if (!parser) {
            qWarning()
                << "[STRUCTURE] Failed to create Tree-sitter parser.";

            return {};
        }

        if (!ts_parser_set_language(
                parser,
                tree_sitter_markdown())) {
            qWarning()
                << "[STRUCTURE] Failed to set Markdown language.";

            ts_parser_delete(
                parser);

            parser =
                nullptr;

            return {};
        }
    }

    const QByteArray newUtf8 =
        text.toUtf8();

    const QByteArray oldUtf8 =
        previousText.toUtf8();

    if (tree &&
        previousText != text) {
        const TSInputEdit inputEdit =
            computeEdit(
                oldUtf8,
                newUtf8);

        ts_tree_edit(
            tree,
            &inputEdit);
    }

    TSTree *newTree =
        ts_parser_parse_string(
            parser,
            tree,
            newUtf8.constData(),
            static_cast<uint32_t>(
                newUtf8.size()));

    if (!newTree) {
        qWarning()
            << "[STRUCTURE] Markdown incremental parse failed.";

        return {};
    }

    if (tree &&
        tree != newTree) {
        ts_tree_delete(
            tree);
    }

    tree =
        newTree;

    DocumentStructure structure;

    structure.setText(
        text);

    const TSNode rootNode =
        ts_tree_root_node(
            tree);

    DocumentNode root =
        makeNode(
            rootNode);

    appendChildren(
        rootNode,
        root);

    structure.setRoot(
        std::move(root));

    return structure;
}

void MarkdownStructureParser::
destroyParser(
    TSParser *&parser) {
    if (!parser) {
        return;
    }

    ts_parser_delete(
        parser);

    parser =
        nullptr;
}

void MarkdownStructureParser::
destroyTree(
    TSTree *&tree) {
    if (!tree) {
        return;
    }

    ts_tree_delete(
        tree);

    tree =
        nullptr;
}

DocumentNode MarkdownStructureParser::makeNode(
    const TSNode &node) {
    DocumentNode result;

    if (ts_node_is_null(node)) {
        return result;
    }

    result.id =
        nodeId(node);

    result.type =
        QString::fromUtf8(
            ts_node_type(node));

    result.start =
        static_cast<int>(
            ts_node_start_byte(node));

    result.end =
        static_cast<int>(
            ts_node_end_byte(node));

    return result;
}

void MarkdownStructureParser::appendChildren(
    const TSNode &node,
    DocumentNode &parent) {
    const uint32_t childCount =
        ts_node_named_child_count(
            node);

    for (uint32_t i = 0;
         i < childCount;
         ++i) {
        const TSNode child =
            ts_node_named_child(
                node,
                i);

        if (ts_node_is_null(child)) {
            continue;
        }

        DocumentNode childNode =
            makeNode(
                child);

        appendChildren(
            child,
            childNode);

        parent.children.append(
            std::move(childNode));
    }
}