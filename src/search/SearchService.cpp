#include "../../include/search/SearchService.h"

#include "../../include/search/ScopeIndex.h"
#include "../../include/search/VectorIndex.h"
#include "inference/InferenceService.h"

#include <QDebug>

SearchService::SearchService(InferenceService *inference,
                             ScopeIndex *index,
                             QObject *parent)
    : QObject(parent), m_inference(inference), m_index(index) {}

SearchService::~SearchService() = default;

bool SearchService::isReady() const {
  return m_inference && m_inference->isEmbedderReady() &&
         m_index && m_index->isReady();
}

QVector<SearchHit> SearchService::search(const QString &query,
                                         int k) const {
  QVector<SearchHit> hits;

  if (query.trimmed().isEmpty() || !isReady() || k <= 0) {
    return hits;
  }

  const std::vector<float> queryVector = m_inference->embed(query);

  if (queryVector.empty()) {
    qWarning() << "[SearchService] Failed to embed query";
    return hits;
  }

  VectorIndex *vectors = m_index->vectors();

  if (!vectors) {
    return hits;
  }

  const QVector<VectorIndex::Hit> raw = vectors->search(queryVector, k);

  hits.reserve(raw.size());

  for (const VectorIndex::Hit &hit : raw) {
    const ScopeIndex::Entry entry = m_index->entryFor(hit.id);

    if (entry.filePath.isEmpty()) {
      continue;
    }

    SearchHit result;
    result.filePath = entry.filePath;
    result.scopeId = entry.scopeId;
    result.heading = entry.heading;
    result.body = entry.body;
    result.similarity = hit.similarity;
    result.vectorId = hit.id;

    hits.append(result);
  }

  return hits;
}