#include "PlainTextStructureParser.h"

namespace {

constexpr int kMaxScopeSize = 700;

void appendChunks(DocumentNode &parent, const QString &text) {
  int position = parent.start;
  int chunkIndex = 0;

  while (position < parent.end) {
    int chunkEnd = qMin(position + kMaxScopeSize, parent.end);

    if (chunkEnd < parent.end) {
      int boundary = chunkEnd;

      while (boundary > position && !text.at(boundary - 1).isSpace()) {
        --boundary;
      }

      if (boundary > position) {
        chunkEnd = boundary;
      }
    }

    while (chunkEnd < parent.end && text.at(chunkEnd).isSpace()) {
      ++chunkEnd;
    }

    DocumentNode chunk;

    chunk.id = QStringLiteral("%1.c%2").arg(parent.id).arg(chunkIndex++);

    chunk.type = QStringLiteral("chunk");

    chunk.start = position;
    chunk.end = chunkEnd;

    parent.children.append(std::move(chunk));

    position = chunkEnd;
  }
}

} // namespace

DocumentStructure PlainTextStructureParser::parse(const QString &text) const {
  DocumentStructure structure;

  structure.setText(text);

  DocumentNode root;

  root.id = QStringLiteral("document");

  root.type = QStringLiteral("document");

  root.start = 0;
  root.end = static_cast<int>(text.size());

  int paragraphIndex = 0;
  int position = 0;

  while (position < text.size()) {
    while (position < text.size() && text.at(position).isSpace()) {
      ++position;
    }

    if (position >= text.size()) {
      break;
    }

    const int paragraphStart = position;

    while (position < text.size()) {
      if (text.at(position) == QLatin1Char('\n')) {
        int next = position + 1;

        while (next < text.size() && (text.at(next) == QLatin1Char('\n') ||
                                      text.at(next) == QLatin1Char('\r'))) {
          ++next;
        }

        if (next > position + 1) {
          break;
        }
      }

      ++position;
    }

    int paragraphEnd = position;

    while (paragraphEnd > paragraphStart &&
           text.at(paragraphEnd - 1).isSpace()) {
      --paragraphEnd;
    }

    if (paragraphEnd <= paragraphStart) {
      ++position;
      continue;
    }

    DocumentNode paragraph;

    paragraph.id = QStringLiteral("p%1").arg(paragraphIndex++);

    paragraph.type = QStringLiteral("paragraph");

    paragraph.start = paragraphStart;

    paragraph.end = paragraphEnd;

    /*
     * Plain text has no syntax tree, so paragraphs are the natural
     * structural scopes. Large paragraphs receive bounded child
     * scopes so the LLM never has to search an enormous region.
     */
    if (paragraph.end - paragraph.start > kMaxScopeSize) {
      appendChunks(paragraph, text);
    }

    root.children.append(std::move(paragraph));

    position = qMax(position + 1, paragraphEnd);
  }

  structure.setRoot(std::move(root));

  return structure;
}