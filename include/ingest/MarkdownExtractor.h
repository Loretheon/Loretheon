#ifndef EPISTEME_MARKDOWNEXTRACTOR_H
#define EPISTEME_MARKDOWNEXTRACTOR_H

#include "Extractor.h"

#include <QHash>
#include <QMutex>

class MarkdownExtractor : public Extractor {
public:
  MarkdownExtractor() = default;
  ~MarkdownExtractor() override = default;

  QString name() const override;
  QStringList extensions() const override;

  bool isPassthrough() const override { return true; }

  quint64 extract(const QString &path, const IngestOptions &options,
                  Callback done,
                  ProgressCallback progress = nullptr) override;

  void cancel(quint64 token) override;

private:
  mutable QMutex m_mutex;
  QHash<quint64, bool> m_cancelled;
  quint64 m_nextToken = 1;
};

#endif // EPISTEME_MARKDOWNEXTRACTOR_H