#pragma once

#include "SearchHit.h"
#include "inference/InferenceService.h"

#include <QObject>
#include <QString>
#include <QVector>

class InferenceService;
class SearchService;
class StructuredCall;

// Agnostic retrieval loop. Takes a query, progressively fetches more
// search results, and asks the LLM after each round whether the
// accumulated notes are enough to answer. When they are, a final call
// produces the answer and streams it back.
//
// Knows nothing about any UI. Consumers connect to the signals.
class RetrievalLoop : public QObject {
  Q_OBJECT

public:
  explicit RetrievalLoop(SearchService *search,
                         InferenceService *inference,
                         QObject *parent = nullptr);
  ~RetrievalLoop() override;

  void setMaxResults(int max);
  int maxResults() const { return m_maxResults; }

  void start(const QString &query);
  void cancel();

  bool isRunning() const { return m_running; }

  signals:
    void stageChanged(const QString &label);
  void answerChunk(const QString &text);
  void sourcesUpdated(const QVector<SearchHit> &hits);
  void finished(const QString &answer);
  void failed(const QString &reason);

private:
  enum class Stage { Idle, Judging, Answering };

  void runJudgmentRound();
  void onJudgmentResult(bool ok, bool sufficient, const QString &reason);

  void runFinalAnswer();
  void onAnswerChunk(const QString &text);
  void onAnswerFinished(const QString &text);

  QString buildAnswerPrompt() const;
  QString formatSources() const;

  SearchService *m_search = nullptr;
  InferenceService *m_inference = nullptr;

  QString m_query;
  QVector<SearchHit> m_hits;
  int m_target = 1;
  int m_maxResults = 8;

  Stage m_stage = Stage::Idle;
  bool m_running = false;

  StructuredCall *m_judgmentCall = nullptr;
  InferenceService::RequestToken m_answerToken;
  QString m_answerBuffer;
};

