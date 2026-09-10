#include "DocumentStructure.h"

#include <QDebug>

namespace {

QString previewText(
    const QString &text,
    int start,
    int end,
    int maximumCharacters) {
    if (start < 0 ||
        end < start ||
        start > text.size()) {
        return {};
    }

    const int safeEnd =
        qMin(
            end,
            text.size());

    QString preview =
        text.mid(
            start,
            safeEnd - start)
            .simplified();

    if (preview.size() <=
        maximumCharacters) {
        return preview;
    }

    return preview.left(
               maximumCharacters - 3) +
           QStringLiteral("...");
}

} // namespace

void DocumentStructure::clear() {
    m_text.clear();
    m_root =
        DocumentNode();
}

void DocumentStructure::setText(
    const QString &text) {
    m_text = text;
}

void DocumentStructure::setRoot(
    DocumentNode root) {
    m_root =
        std::move(root);
}

const QString &DocumentStructure::text() const {
    return m_text;
}

const DocumentNode &
DocumentStructure::root() const {
    return m_root;
}

const DocumentNode *
DocumentStructure::find(
    const QString &id) const {
    qDebug()
        << "DocumentStructure::find:"
        << "requested id ="
        << id;

    if (id.isEmpty()) {
        qDebug()
            << "DocumentStructure::find:"
            << "empty id";

        return nullptr;
    }

    const DocumentNode *result =
        findRecursive(
            m_root,
            id);

    if (!result) {
        qDebug()
            << "DocumentStructure::find:"
            << "NOT FOUND:"
            << id;

        qDebug().noquote()
            << "Available structure:\n"
            << indexForModel();
    } else {
        qDebug()
            << "DocumentStructure::find:"
            << "FOUND:"
            << result->id
            << "type ="
            << result->type
            << "range ="
            << result->start
            << "-"
            << result->end;

        qDebug().noquote()
            << "Matched text:"
            << previewText(
                   m_text,
                   result->start,
                   result->end,
                   120);
    }

    return result;
}

const DocumentNode *
DocumentStructure::findRecursive(
    const DocumentNode &node,
    const QString &id) const {
    if (node.id == id) {
        return &node;
    }

    for (const DocumentNode &child :
         node.children) {
        const DocumentNode *result =
            findRecursive(
                child,
                id);

        if (result) {
            return result;
        }
    }

    return nullptr;
}

QString DocumentStructure::indexForModel(
    int maxPreviewCharacters) const {
    QString output;

    if (m_root.id.isEmpty()) {
        return output;
    }

    appendIndexRecursive(
        m_root,
        output,
        0,
        maxPreviewCharacters);

    return output;
}

void DocumentStructure::appendIndexRecursive(
    const DocumentNode &node,
    QString &output,
    int depth,
    int maxPreviewCharacters) const {
    for (int i = 0;
         i < depth;
         ++i) {
        output +=
            QStringLiteral("  ");
    }

    QString line =
        QStringLiteral("%1 [%2]")
            .arg(node.id)
            .arg(node.type);

    if (node.start >= 0 &&
        node.end >= node.start &&
        node.end <= m_text.size() &&
        node.end > node.start) {
        const QString preview =
            previewText(
                m_text,
                node.start,
                node.end,
                maxPreviewCharacters);

        if (!preview.isEmpty()) {
            line +=
                QStringLiteral(" \"%1\"")
                    .arg(preview);
        }
    }

    output += line;
    output += QChar('\n');

    for (const DocumentNode &child :
         node.children) {
        appendIndexRecursive(
            child,
            output,
            depth + 1,
            maxPreviewCharacters);
    }
}