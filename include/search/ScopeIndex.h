#pragma once

#include "SearchHit.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <memory>

class InferenceService;
class VectorIndex;

// Walks a notes directory, extracts scopes from each Markdown file,
// embeds them, and stores the vectors in a FAISS index with a JSON
// sidecar that maps vector id back to file and scope.
//
// The index lives under a caller-provided directory. That directory
// holds two files:
//
//   faiss.index     — the FAISS index itself
//   sidecar.json    — one entry per vector: filePath, scopeId, heading,
//                     body, contentHash
//
// The sidecar format is versioned. If the version changes, the index
// is rebuilt from scratch on next load.
class ScopeIndex : public QObject {
  Q_OBJECT

public:
  struct Entry {
    QString filePath;
    QString scopeId;
    QString heading;
    QString body;
    QString contentHash;
  };

  explicit ScopeIndex(InferenceService *inference,
                      QObject *parent = nullptr);
  ~ScopeIndex() override;

  // Where the index files live. Must be set before any other call.
  void setIndexDirectory(const QString &directory);
  QString indexDirectory() const { return m_indexDirectory; }

  // Load from disk if present, otherwise start empty. Returns true if
  // an index was successfully loaded.
  bool load();

  // Full rebuild: clear, walk the directory, embed every scope, save.
  // Returns the number of scopes indexed, or -1 on failure.
  int rebuild(const QString &notesRoot);

  // True if the index has at least one vector and the embedder is ready.
  bool isReady() const;

  int64_t vectorCount() const;
  int64_t scopeCount() const;

  // Look up an entry by vector id. Returns an invalid entry if not found.
  Entry entryFor(int64_t vectorId) const;

  // The FAISS index. Used by SearchService.
  VectorIndex *vectors() const { return m_vectors.get(); }

signals:
  // Progress during a rebuild. current and total are scope counts.
  void progress(int current, int total);

  // Emitted once when a rebuild finishes. scopes is the count, or -1
  // on failure.
  void finished(int scopes);

private:
  // Recursively collect .md files under root.
  QStringList collectMarkdownFiles(const QString &root) const;

  // Extract scopes from a single file. Returns an empty list on failure.
  QVector<Entry> extractScopes(const QString &absolutePath) const;

  // Recurse into a DocumentNode tree, emitting one entry per valid
  // non-root node with a body.
  void collectNodes(const struct DocumentNode &node,
                    const QString &documentText,
                    const QString &filePath,
                    QVector<Entry> &out) const;

  bool saveSidecar(const QString &path) const;
  bool loadSidecar(const QString &path);

  InferenceService *m_inference = nullptr;

  QString m_indexDirectory;

  std::unique_ptr<VectorIndex> m_vectors;
  QVector<Entry> m_entries;   // index by vector id

  int m_dimensions = 384;
  static constexpr int kSidecarVersion = 1;
};