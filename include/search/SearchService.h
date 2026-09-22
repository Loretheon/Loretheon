#pragma once

#include "SearchHit.h"

#include <QObject>
#include <QString>
#include <QVector>

class InferenceService;
class ScopeIndex;

// Turns a query string into ranked search hits.
//
// The pipeline is: embed the query, ask FAISS for the nearest scopes,
// resolve each vector id to a sidecar entry, return the hits.
//
// No LLM is involved. The LLM only enters when the caller decides to
// hand the top hits to a chat prompt.
class SearchService : public QObject {
  Q_OBJECT

public:
  explicit SearchService(InferenceService *inference,
                         ScopeIndex *index,
                         QObject *parent = nullptr);
  ~SearchService() override;

  // Run a query and return up to k hits, best first. An empty result
  // means either no index or no matches.
  QVector<SearchHit> search(const QString &query, int k = 8) const;

  // True if the underlying index is loaded and the embedder is ready.
  bool isReady() const;

private:
  InferenceService *m_inference = nullptr;
  ScopeIndex *m_index = nullptr;
};