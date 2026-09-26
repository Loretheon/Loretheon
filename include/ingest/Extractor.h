#ifndef EPISTEME_EXTRACTOR_H
#define EPISTEME_EXTRACTOR_H

#include "ExtractedDocument.h"
#include "IngestOptions.h"

#include <QString>
#include <QStringList>

#include <functional>
#include <memory>

class Extractor {
public:
  virtual ~Extractor() = default;

  virtual QString name() const = 0;

  virtual QStringList extensions() const = 0;

  // True when the source is already a note and should be copied into
  // the destination folder unchanged. IngestService skips NoteWriter
  // composition for passthrough extractors: no provenance block, no
  // title heading, no section splitting. The bytes land as they are.
  virtual bool isPassthrough() const { return false; }

  struct Result {
    ExtractedDocument document;
    QString error;
    bool ok() const { return error.isEmpty(); }
  };

  using Callback = std::function<void(Result)>;

  using ProgressCallback = std::function<void(double, const QString &)>;

  virtual quint64 extract(const QString &path, const IngestOptions &options,
                          Callback done,
                          ProgressCallback progress = nullptr) = 0;

  virtual void cancel(quint64 token) = 0;
};

#endif // EPISTEME_EXTRACTOR_H