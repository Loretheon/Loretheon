#include "../../include/ingest/IngestRegistry.h"
#include "../../include/ingest/Extractors.h"

#include <QFileInfo>

IngestRegistry::IngestRegistry() = default;

void IngestRegistry::add(Extractor *extractor) {
  if (extractor) {
    m_borrowed.push_back(extractor);
  }
}

void IngestRegistry::take(std::unique_ptr<Extractor> extractor) {
  if (extractor) {
    m_owned.push_back(std::move(extractor));
  }
}

Extractor *IngestRegistry::forExtension(const QString &extension) const {
  QString key = extension.toLower();
  if (key.startsWith(QLatin1Char('.'))) {
    key.remove(0, 1);
  }
  if (key.isEmpty()) {
    return nullptr;
  }

  for (Extractor *extractor : m_borrowed) {
    if (extractor && extractor->extensions().contains(key)) {
      return extractor;
    }
  }
  for (const auto &extractor : m_owned) {
    if (extractor && extractor->extensions().contains(key)) {
      return extractor.get();
    }
  }
  return nullptr;
}

Extractor *IngestRegistry::forPath(const QString &path) const {
  return forExtension(QFileInfo(path).suffix());
}

bool IngestRegistry::canHandle(const QString &path) const {
  return forPath(path) != nullptr;
}

QStringList IngestRegistry::knownExtensions() const {
  QStringList result;
  for (Extractor *extractor : m_borrowed) {
    if (extractor) {
      for (const QString &ext : extractor->extensions()) {
        if (!result.contains(ext)) {
          result.append(ext);
        }
      }
    }
  }
  for (const auto &extractor : m_owned) {
    if (extractor) {
      for (const QString &ext : extractor->extensions()) {
        if (!result.contains(ext)) {
          result.append(ext);
        }
      }
    }
  }
  return result;
}

bool IngestRegistry::isEmpty() const {
  return m_borrowed.empty() && m_owned.empty();
}

void registerBuiltinExtractors(IngestRegistry &registry) {
  registry.take(makePdfExtractor());
  registry.take(makeHtmlExtractor());
  registry.take(makeDocxExtractor());
  registry.take(makePptxExtractor());
  registry.take(makeEpubExtractor());
}