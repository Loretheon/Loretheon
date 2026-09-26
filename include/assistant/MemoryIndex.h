#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

#include <memory>

class InferenceService;
class VectorIndex;

// A second, private index over the assistant's own memory directory.
// Structurally separate from the notes index. The user never searches
// this. Only AssistantMemory::recall reads it.
//
// Index files live alongside the memory root, not with the notes index:
//
//   <assistantRoot>/memories/.index/faiss.index
//   <assistantRoot>/memories/.index/sidecar.json
//
// Same value types as ScopeIndex but a distinct class so a bug in one
// cannot leak into the other.
class MemoryIndex : public QObject {
  Q_OBJECT

public:
  struct Entry {
    QString filePath;
    QString scopeId;
    QString heading;
    QString body;
    QString contentHash;
  };

  explicit MemoryIndex(InferenceService *inference,
                       QObject *parent = nullptr);
  ~MemoryIndex() override;

  // The memory root to walk. Usually <assistantRoot>/memories. The
  // index files go in a .index subdirectory of this path.
  void setMemoryRoot(const QString &root);
  QString memoryRoot() const { return m_memoryRoot; }

  bool load();
  int rebuild();

  bool isReady() const;

  int64_t vectorCount() const;
  int64_t scopeCount() const;

  Entry entryFor(int64_t vectorId) const;

  VectorIndex *vectors() const { return m_vectors.get(); }

signals:
  void progress(int current, int total);
  void finished(int scopes);

private:
  QString indexDirectory() const;

  QStringList collectMarkdownFiles() const;

  QVector<Entry> extractScopes(const QString &absolutePath) const;

  void collectNodes(const struct DocumentNode &node,
                    const QString &documentText,
                    const QString &filePath,
                    QVector<Entry> &out) const;

  bool saveSidecar(const QString &path) const;
  bool loadSidecar(const QString &path);

  InferenceService *m_inference = nullptr;

  QString m_memoryRoot;

  std::unique_ptr<VectorIndex> m_vectors;
  QVector<Entry> m_entries;

  int m_dimensions = 384;
  static constexpr int kSidecarVersion = 1;
};