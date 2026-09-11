#include "PlainTextStructureParser.h"

namespace {

constexpr int MaxScopeSize = 700;

DocumentNode makeChunk(const DocumentNode &parent, int start, int end,
                       int index) {
  DocumentNode chunk;

  chunk.id = QStringLiteral("%1.c%2").arg(parent.id).arg(index);

  chunk.type = QStringLiteral("chunk");
  chunk.start = start;
  chunk.end = end;

  return chunk;
}

void appendChunks(DocumentNode &parent, const QString &text) {
  int position = parent.start;
  int index = 0;

  while (position < parent.end) {
    int end = qMin(position + MaxScopeSize, parent.end);

    if (end < parent.end) {
      int boundary = end;

      while (boundary > position && !text.at(boundary - 1).isSpace()) {
        --boundary;
      }

      if (boundary > position) {
        end = boundary;
      }
    }

    while (end < parent.end && text.at(end).isSpace()) {
      ++end;
    }

    parent.children.append(makeChunk(parent, position, end, index++));

    position = end;
  }
}

int skipWhitespace(const QString &text, int position) {
  while (position < text.size() && text.at(position).isSpace()) {
    ++position;
  }

  return position;
}

int findParagraphEnd(const QString &text, int position) {
  while (position < text.size()) {
    if (text.at(position) != QLatin1Char('\n')) {
      ++position;
      continue;
    }

    int next = position + 1;

    while (next < text.size() && (text.at(next) == QLatin1Char('\n') ||
                                  text.at(next) == QLatin1Char('\r'))) {
      ++next;
    }

    if (next > position + 1) {
      break;
    }

    ++position;
  }

  return position;
}

} // namespace

DocumentStructure PlainTextStructureParser::parse(const QString &text) const {
  DocumentStructure structure;
  structure.setText(text);

  DocumentNode root;

  root.id = QStringLiteral("document");
  root.type = QStringLiteral("document");
  root.start = 0;
  root.end = text.size();

  int paragraphIndex = 0;
  int position = 0;

  while (position < text.size()) {
    position = skipWhitespace(text, position);

    if (position >= text.size()) {
      break;
    }

    const int paragraphStart = position;
    const int paragraphBoundary = findParagraphEnd(text, position);

    int paragraphEnd = paragraphBoundary;

    while (paragraphEnd > paragraphStart &&
           text.at(paragraphEnd - 1).isSpace()) {
      --paragraphEnd;
    }

    if (paragraphEnd <= paragraphStart) {
      position = qMax(position + 1, paragraphBoundary);
      continue;
    }

    DocumentNode paragraph;

    paragraph.id = QStringLiteral("p%1").arg(paragraphIndex++);

    paragraph.type = QStringLiteral("paragraph");
    paragraph.start = paragraphStart;
    paragraph.end = paragraphEnd;

    if (paragraph.end - paragraph.start > MaxScopeSize) {
      appendChunks(paragraph, text);
    }

    root.children.append(std::move(paragraph));

    position = qMax(paragraphBoundary + 1, paragraphEnd);
  }

  structure.setRoot(std::move(root));
  return structure;
}