#include "../../include/ingest/ZipReader.h"

#include <quazip/quazip.h>
#include <quazip/quazipfile.h>

#include <QFile>

namespace {

class QuaZipReader final : public ZipReader {
public:
  explicit QuaZipReader(const QString &path) : m_zip(path) {
    m_ok = m_zip.open(QuaZip::mdUnzip);
    if (m_ok) {
      m_names = m_zip.getFileNameList();
    }
  }

  ~QuaZipReader() override {
    if (m_ok) {
      m_zip.close();
    }
  }

  QStringList entries() const override { return m_names; }

  bool contains(const QString &entry) const override {
    return m_names.contains(entry);
  }

  QByteArray read(const QString &entry, QString &error) const override {
    if (!m_ok) {
      error = QStringLiteral("ZIP archive is not open.");
      return {};
    }
    if (!m_names.contains(entry)) {
      error = QStringLiteral("Entry not found: %1").arg(entry);
      return {};
    }

    m_zip.setCurrentFile(entry);
    QuaZipFile file(&m_zip);
    if (!file.open(QIODevice::ReadOnly)) {
      error = QStringLiteral("Failed to open entry: %1").arg(entry);
      return {};
    }

    const QByteArray data = file.readAll();
    file.close();

    if (file.getZipError() != UNZ_OK) {
      error = QStringLiteral("Error reading entry: %1").arg(entry);
      return {};
    }

    return data;
  }

private:
  mutable QuaZip m_zip;
  QStringList m_names;
  bool m_ok = false;
};

} // namespace

std::unique_ptr<ZipReader> ZipReader::open(const QString &path,
                                           QString &error) {
  if (!QFile::exists(path)) {
    error = QStringLiteral("No such file: %1").arg(path);
    return nullptr;
  }

  auto reader = std::make_unique<QuaZipReader>(path);
  if (!reader->entries().isEmpty() || QFileInfo(path).size() == 0) {
    return reader;
  }

  // An archive with entries but an empty name list, or a non-empty file
  // that QuaZip rejected, is a failure.
  error = QStringLiteral("Not a valid ZIP archive: %1").arg(path);
  return nullptr;
}