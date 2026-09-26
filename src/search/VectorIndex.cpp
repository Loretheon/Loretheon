#include "../../include/search/VectorIndex.h"

#include <QDebug>
#include <QFileInfo>

#include <faiss/IndexFlat.h>
#include <faiss/impl/AuxIndexStructures.h>
#include <faiss/impl/IDSelector.h>
#include <faiss/index_io.h>

#include <unordered_set>

namespace {

class IdSetSelector : public faiss::IDSelector {
public:
  explicit IdSetSelector(
      const std::unordered_set<faiss::Index::idx_t> &ids)
      : m_ids(ids) {}

  bool is_member(faiss::Index::idx_t id) const override {
    return m_ids.count(id) > 0;
  }

private:
  const std::unordered_set<faiss::Index::idx_t> &m_ids;
};

} // namespace

VectorIndex::VectorIndex() = default;

VectorIndex::~VectorIndex() = default;

VectorIndex::VectorIndex(VectorIndex &&other) noexcept
    : m_index(std::move(other.m_index)),
      m_dimensions(other.m_dimensions) {
  other.m_dimensions = 0;
}

VectorIndex &VectorIndex::operator=(VectorIndex &&other) noexcept {
  if (this == &other) {
    return *this;
  }

  m_index = std::move(other.m_index);
  m_dimensions = other.m_dimensions;

  other.m_dimensions = 0;

  return *this;
}

bool VectorIndex::create(int dimensions) {
  if (dimensions <= 0) {
    return false;
  }

  m_index = std::make_unique<faiss::IndexFlatIP>(dimensions);
  m_dimensions = dimensions;

  return true;
}

int64_t VectorIndex::add(const std::vector<float> &vector) {
  if (!m_index) {
    return -1;
  }

  if (static_cast<int>(vector.size()) != m_dimensions) {
    return -1;
  }

  const int64_t id = m_index->ntotal;

  m_index->add(1, vector.data());

  return id;
}

int VectorIndex::removeIds(const QVector<int64_t> &ids) {
  if (!m_index || ids.isEmpty()) {
    return 0;
  }

  std::unordered_set<faiss::Index::idx_t> set;
  set.reserve(static_cast<size_t>(ids.size()));

  for (int64_t id : ids) {
    set.insert(static_cast<faiss::Index::idx_t>(id));
  }

  IdSetSelector selector(set);

  const size_t removed = m_index->remove_ids(selector);

  return static_cast<int>(removed);
}

QVector<VectorIndex::Hit> VectorIndex::search(
    const std::vector<float> &query, int k) const {
  QVector<Hit> result;

  if (!m_index || k <= 0) {
    return result;
  }

  if (static_cast<int>(query.size()) != m_dimensions) {
    return result;
  }

  const int count = qMin<int>(k, static_cast<int>(m_index->ntotal));

  if (count <= 0) {
    return result;
  }

  std::vector<faiss::Index::idx_t> ids(count);
  std::vector<float> distances(count);

  m_index->search(1, query.data(), count, distances.data(), ids.data());

  result.reserve(count);

  for (int i = 0; i < count; ++i) {
    Hit hit;
    hit.id = static_cast<int64_t>(ids[i]);
    hit.similarity = distances[i];
    result.append(hit);
  }

  return result;
}

bool VectorIndex::save(const QString &path) const {
  if (!m_index || path.isEmpty()) {
    return false;
  }

  try {
    faiss::write_index(m_index.get(), path.toStdString().c_str());
  } catch (const std::exception &e) {
    qWarning() << "[VectorIndex] Save failed:" << e.what();
    return false;
  }

  return QFileInfo::exists(path);
}

bool VectorIndex::load(const QString &path) {
  if (path.isEmpty() || !QFileInfo::exists(path)) {
    return false;
  }

  try {
    m_index.reset(faiss::read_index(path.toStdString().c_str()));
  } catch (const std::exception &e) {
    qWarning() << "[VectorIndex] Load failed:" << e.what();
    m_index.reset();
    m_dimensions = 0;
    return false;
  }

  if (!m_index) {
    m_dimensions = 0;
    return false;
  }

  m_dimensions = static_cast<int>(m_index->d);

  return true;
}

int64_t VectorIndex::size() const {
  return m_index ? m_index->ntotal : 0;
}

int VectorIndex::dimensions() const { return m_dimensions; }

bool VectorIndex::isValid() const {
  return m_index != nullptr && m_dimensions > 0;
}

void VectorIndex::clear() {
  if (!m_index) {
    return;
  }

  m_index->reset();
}