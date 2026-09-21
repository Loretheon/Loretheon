#ifndef EPISTEME_EXTRACTEDDOCUMENT_H
#define EPISTEME_EXTRACTEDDOCUMENT_H

#include <QDateTime>
#include <QString>
#include <QVector>

// One logical unit of extracted content: a page, a slide, a chapter, a
// heading-delimited block. `level` maps to Markdown heading depth
// (1 = "#", 2 = "##", ...). `heading` may be empty for untitled sections.
struct ExtractedSection {
  QString heading;
  QString body;
  int level = 2;
};

// Everything an extractor learns about the source file that is worth
// recording in the note's provenance block.
struct ExtractedMetadata {
  QString sourcePath;      // absolute path the file was read from
  QString sourceName;      // file name only
  QString sourceHash;      // SHA-1 of the source bytes, hex
  QDateTime extractedAt;   // UTC
  int pageCount = 0;       // pages, slides, chapters - best effort
  QString mimeType;        // optional; empty if unknown
};

// The value type every extractor produces. No behaviour, no ownership.
struct ExtractedDocument {
  QString title;                      // suggested note title
  QVector<ExtractedSection> sections; // may be empty for empty sources
  ExtractedMetadata metadata;

  bool isEmpty() const { return sections.isEmpty() && title.isEmpty(); }
};

#endif // EPISTEME_EXTRACTEDDOCUMENT_H