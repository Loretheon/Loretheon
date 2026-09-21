#ifndef EPISTEME_INGESTREGISTRY_H
#define EPISTEME_INGESTREGISTRY_H

#include "Extractor.h"

#include <QString>
#include <QStringList>

#include <memory>
#include <vector>

// Maps file extensions to extractors. The registry owns nothing it did
// not construct via take(); borrowed extractors registered with add()
// must outlive the registry. Lookup is by lower-case extension.
//
// Open/closed: adding a format means registering an extractor, not
// editing this class.
class IngestRegistry {
public:
  IngestRegistry();

  // Borrowed registration: the registry does not own the extractor.
  void add(Extractor *extractor);

  // Owned registration: the registry takes ownership.
  void take(std::unique_ptr<Extractor> extractor);

  // Returns nullptr when no extractor is registered for the extension.
  // `extension` may be given with or without a leading dot, in any case.
  Extractor *forExtension(const QString &extension) const;

  // Convenience: extractor for a path, or nullptr.
  Extractor *forPath(const QString &path) const;

  // True if any registered extractor claims this extension.
  bool canHandle(const QString &path) const;

  // Every extension any registered extractor claims, lower-case, no dot.
  QStringList knownExtensions() const;

  // True when no extractor has been registered.
  bool isEmpty() const;

private:
  std::vector<Extractor *> m_borrowed;
  std::vector<std::unique_ptr<Extractor>> m_owned;
};

// Registers every built-in extractor with `registry`. Call once at
// startup, before any import is attempted.
void registerBuiltinExtractors(IngestRegistry &registry);

#endif // EPISTEME_INGESTREGISTRY_H