#ifndef EPISTEME_INGESTOPTIONS_H
#define EPISTEME_INGESTOPTIONS_H

#include <QString>

// Per-import user choices. Defaults are the common case: convert the
// source into a note, write a provenance block, derive the note name
// from the source. The source file is never modified or removed; import
// reads it and writes a new note.
struct IngestOptions {
  // Where the resulting note is written.
  QString destinationFolder;

  // Override the derived note name. Empty means "derive from source".
  QString noteNameOverride;

  // Whether to write the provenance frontmatter block.
  bool writeProvenance = true;

  // Per-page (PDF) / per-slide (PPTX) headings vs. a single flat body.
  // Honoured by extractors that have a natural pagination unit.
  bool sectionPerPage = true;
};

#endif // EPISTEME_INGESTOPTIONS_H