#include "../../include/search/VectorIndex.h"

#include <QDebug>
#include <QFile>

#include <faiss/IndexFlat.h>
#include <faiss/index_io.h>

VectorIndex::VectorIndex() = default;

VectorIndex::~VectorIndex() = default;

bool VectorIndex::create(int dimensions) {
  if (dimensions <= 0) {
    qWarning() << "[VectorIndex] Invalid dimensions:" << dimensions;
    return false;
  }

  // IndexFlatIP uses inner product. With normalized vectors, that is
  // cosine similarity.
  m_index = std::make_unique<faiss::IndexFlatIP>(dimensions);
  m_dimensions = dimensions;

  return true;
}

int64_t VectorIndex::add(const std::vector<float> &vector) {
  if (!m_index) {
    return -1;
  }

  if (static_cast<int>(vector.size()) != m_dimensions) {
    qWarning() << "[VectorIndex] Vector dimension mismatch:"
               << vector.size() << "vs" << m_dimensions;
    return -1;
  }

  const int64_t id = m_index->ntotal;

  m_index->add(1, vector.data());

  return id;
}

QVector<VectorIndex::Hit> VectorIndex::search(
    const std::vector<float> &query, int k) const {
  QVector<Hit> result;

  if (!m_index || k <= 0) {
    return result;
  }

  if (static_cast<int>(query.size()) != m_dimensions) {
    qWarning() << "[VectorIndex] Query dimension mismatch";
    return result;
  }

  const int64_t total = m_index->ntotal;
  if (total == 0) {
    return result;
  }

  const int actualK = static_cast<int>(std::min<int64_t>(k, total));

  std::vector<float> distances(actualK);
  std::vector<faiss::Index::idx_t> labels(actualK);
  m_index->search(1, query.data(), actualK, distances.data(),
                  labels.data());

  result.reserve(actualK);

  for (int i = 0; i < actualK; ++i) {
    if (labels[i] < 0) {
      continue;
    }

    Hit hit;
    hit.id = labels[i];
    hit.similarity = distances[i];
    result.append(hit);
  }

  return result;
}

bool VectorIndex::save(const QString &path) const {
  if (!m_index) {
    return false;
  }

  try {
    faiss::write_index(m_index.get(), path.toStdString().c_str());
  } catch (const std::exception &e) {
    qWarning() << "[VectorIndex] Save failed:" << e.what();
    return false;
  }

  return true;
}

bool VectorIndex::load(const QString &path) {
  if (!QFile::exists(path)) {
    qWarning() << "[VectorIndex] File not found:" << path;
    return false;
  }

  try {
    m_index = std::unique_ptr<faiss::Index>(
        faiss::read_index(path.toStdString().c_str()));
  } catch (const std::exception &e) {
    qWarning() << "[VectorIndex] Load failed:" << e.what();
    m_index.reset();
    return false;
  }

  if (!m_index) {
    return false;
  }

  m_dimensions = m_index->d;

  return true;
}

int64_t VectorIndex::size() const {
  return m_index ? m_index->ntotal : 0;
}

int VectorIndex::dimensions() const { return m_dimensions; }

bool VectorIndex::isValid() const { return m_index != nullptr; }

void VectorIndex::clear() {
  if (m_index) {
    m_index->reset();
  }
}