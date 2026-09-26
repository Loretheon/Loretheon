#ifndef EPISTEME_ZIPREADER_H
#define EPISTEME_ZIPREADER_H

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <memory>

// Thin interface over a ZIP archive. Exists so the OOXML and EPUB
// extractors do not depend on Qt's private QZipReader directly, and so
// the backing implementation can be swapped without touching them.
class ZipReader {
public:
  virtual ~ZipReader() = default;

  // Open an archive. Returns nullptr and sets `error` on failure.
  static std::unique_ptr<ZipReader> open(const QString &path,
                                         QString &error);

  // File names contained in the archive, in archive order.
  virtual QStringList entries() const = 0;

  // True if the named entry exists.
  virtual bool contains(const QString &entry) const = 0;

  // Read a single entry. Returns an empty array and sets `error` on
  // failure. A legitimately empty file is not an error.
  virtual QByteArray read(const QString &entry, QString &error) const = 0;
};

#endif // EPISTEME_ZIPREADER_H