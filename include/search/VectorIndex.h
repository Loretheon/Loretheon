#pragma once

#include <QString>
#include <QVector>

#include <memory>
#include <vector>

namespace faiss { class Index; }

// Thin wrapper over a FAISS IndexFlatIP. Stores L2-normalized vectors
// so that inner product equals cosine similarity.
//
// The index is exact — brute force — which is correct up to roughly a
// million vectors. Beyond that you would swap to IndexIVFFlat and add
// training. The interface stays the same.
class VectorIndex {
public:
  VectorIndex();
  ~VectorIndex();

  VectorIndex(const VectorIndex &) = delete;
  VectorIndex &operator=(const VectorIndex &) = delete;

  VectorIndex(VectorIndex &&other) noexcept;
  VectorIndex &operator=(VectorIndex &&other) noexcept;

  // Create a fresh index with the given dimensionality. Discards any
  // existing data. Returns false if dimensions <= 0.
  bool create(int dimensions);

  // Append a vector and return its assigned id. The id is sequential
  // and starts at 0. Returns -1 on failure.
  int64_t add(const std::vector<float> &vector);

  // Remove a set of ids. Returns the number actually removed. The
  // remaining vectors keep their existing ids; FAISS compacts the
  // underlying storage without reassigning them.
  int removeIds(const QVector<int64_t> &ids);

  // Search for the nearest k vectors. Returns ids and similarities,
  // best first. Both output vectors are empty on failure.
  struct Hit {
    int64_t id = -1;
    float similarity = 0.0f;
  };
  QVector<Hit> search(const std::vector<float> &query, int k) const;

  // Persist and restore. The index file holds the vectors; the caller
  // is responsible for whatever sidecar maps ids to paths.
  bool save(const QString &path) const;
  bool load(const QString &path);

  // Number of vectors currently stored.
  int64_t size() const;

  // Dimensionality, or 0 if the index has not been created.
  int dimensions() const;

  // True if an index exists and is usable.
  bool isValid() const;

  // Remove all vectors. Keeps the dimensionality.
  void clear();

private:
  std::unique_ptr<faiss::Index> m_index;
  int m_dimensions = 0;
};