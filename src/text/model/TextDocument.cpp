#include "TextDocument.h"

#include "../structure/MarkdownStructureParser.h"
#include "../structure/PlainTextStructureParser.h"

#include <QDebug>
#include <QPlainTextDocumentLayout>

TextDocument::TextDocument(
    QObject *parent)
    : QTextDocument(parent) {
    setDocumentLayout(
        new QPlainTextDocumentLayout(this));
}

TextDocument::~TextDocument() {
    MarkdownStructureParser::
        destroyParser(
            m_markdownParser);

    MarkdownStructureParser::
        destroyTree(
            m_markdownTree);
}

QString TextDocument::filePath() const {
    return path;
}

void TextDocument::setFilePath(
    const QString &newPath) {
    path =
        newPath;
}

DocumentMode TextDocument::type() const {
    return docType;
}

void TextDocument::setType(
    DocumentMode newType) {
    if (docType == newType) {
        return;
    }

    qDebug()
        << "TextDocument::setType:"
        << static_cast<int>(newType)
        << "file:"
        << path;

    docType =
        newType;

    m_structureRevision =
        -1;

    MarkdownStructureParser::
        destroyParser(
            m_markdownParser);

    MarkdownStructureParser::
        destroyTree(
            m_markdownTree);
}

const DocumentStructure &
TextDocument::structure() const {
    return m_structure;
}

void TextDocument::rebuildStructure() const {
    const int currentRevision =
        revision();

    if (m_structureRevision ==
        currentRevision) {
        return;
    }

    const QString text =
        toPlainText();

    const QString modeName =
        docType == DocumentMode::Markdown
            ? QStringLiteral("Markdown")
            : docType == DocumentMode::PlainText
                  ? QStringLiteral("PlainText")
                  : QStringLiteral("HTML");

    qDebug()
        << "[STRUCTURE] Rebuilding"
        << "file:"
        << path
        << "mode:"
        << modeName
        << "revision:"
        << currentRevision
        << "length:"
        << text.size();

    switch (docType) {
    case DocumentMode::Markdown: {
        qDebug()
            << "[STRUCTURE] Parser:"
            << "MarkdownStructureParser";

        MarkdownStructureParser parser;

        m_structure =
            parser.parse(
                text,
                m_markdownParser,
                m_markdownTree,
                m_structure.text());

        qDebug()
            << "[STRUCTURE] Markdown root children:"
            << m_structure.root().children.size();

        qDebug().noquote()
            << "[STRUCTURE] Markdown index:\n"
            << (m_structure.indexForModel().isEmpty()
                    ? QStringLiteral("<empty>")
                    : m_structure.indexForModel());

        break;
    }

    case DocumentMode::PlainText: {
        qDebug()
            << "[STRUCTURE] Parser:"
            << "PlainTextStructureParser";

        PlainTextStructureParser parser;

        m_structure =
            parser.parse(
                text);

        break;
    }

    case DocumentMode::Html: {
        qDebug()
            << "[STRUCTURE] Parser:"
            << "PlainTextStructureParser"
            << "(HTML fallback)";

        PlainTextStructureParser parser;

        m_structure =
            parser.parse(
                text);

        break;
    }
    }

    qDebug()
        << "[STRUCTURE] Result:"
        << "root id:"
        << m_structure.root().id
        << "root type:"
        << m_structure.root().type
        << "children:"
        << m_structure.root().children.size();

    m_structureRevision =
        currentRevision;

    qDebug()
        << "[STRUCTURE] Complete"
        << "revision:"
        << m_structureRevision;
}