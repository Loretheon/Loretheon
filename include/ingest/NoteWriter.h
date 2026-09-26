#ifndef EPISTEME_NOTEWRITER_H
#define EPISTEME_NOTEWRITER_H

#include "ExtractedDocument.h"
#include "IngestOptions.h"

#include <QString>

#include <memory>

// Turns an ExtractedDocument into Markdown and writes it. Separated from
// the extractors because "what the note looks like" is a different
// concern from "where the content came from".
//
// Abstract so IngestService can be tested without touching the disk, and
// so a future caller can redirect output elsewhere.
class NoteWriter {
public:
  virtual ~NoteWriter() = default;

  struct Result {
    QString path;   // absolute path written
    QString error;  // empty on success
    bool ok() const { return error.isEmpty(); }
  };

  // Compose the Markdown body for a document. Does not touch the disk.
  // Public so callers can preview the note before writing it.
  static QString compose(const ExtractedDocument &document,
                         const IngestOptions &options);

  // Derive a filesystem-safe note file name from the document and
  // options. Never contains a path separator. Ends in ".md".
  static QString deriveFileName(const ExtractedDocument &document,
                                const IngestOptions &options);

  // Like deriveFileName, but guarantees the returned name does not
  // already exist in `destinationFolder`. Appends " (2)", " (3)", ...
  // before the extension until the name is free. If destinationFolder
  // is empty or does not exist, behaves like deriveFileName.
  static QString deriveUniqueFileName(const ExtractedDocument &document,
                                      const IngestOptions &options,
                                      const QString &destinationFolder);

  // Write the composed note to `absolutePath`, creating parent
  // directories as needed. Overwrites only if `overwrite` is true.
  virtual Result write(const ExtractedDocument &document,
                       const IngestOptions &options,
                       const QString &absolutePath, bool overwrite) = 0;
};

// The default NoteWriter: writes UTF-8 Markdown to disk with QSaveFile.
class DiskNoteWriter final : public NoteWriter {
public:
  DiskNoteWriter() = default;

  Result write(const ExtractedDocument &document,
               const IngestOptions &options, const QString &absolutePath,
               bool overwrite) override;
};

#endif // EPISTEME_NOTEWRITER_H