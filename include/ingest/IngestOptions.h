#ifndef EPISTEME_INGESTOPTIONS_H
#define EPISTEME_INGESTOPTIONS_H

#include <QString>

struct IngestOptions {
  QString destinationFolder;

  // Subfolder under destinationFolder the note is written into. Set
  // when importing a folder tree so the source structure is preserved.
  // Empty means write directly into destinationFolder.
  QString relativeSubpath;

  QString noteNameOverride;

  bool writeProvenance = true;

  bool sectionPerPage = true;
};

#endif // EPISTEME_INGESTOPTIONS_H