#pragma once

#include "EditCommand.h"
#include "EditMatch.h"

#include <QTextDocument>
#include <QVector>

class EditMatcher {
public:
  struct Result {
    QVector<EditMatch> candidates;
    bool fuzzy = false;

    bool isEmpty() const { return candidates.isEmpty(); }

    bool isAmbiguous() const { return candidates.size() > 1; }

    bool hasUniqueMatch() const { return candidates.size() == 1; }
  };

  Result find(const QTextDocument &document, const EditCommand &command) const;

private:
  Result findExact(const QString &text, const QString &needle) const;

  Result findFuzzy(const QString &text, const QString &needle) const;

  static bool isAsciiSafe(const QString &text);

  static constexpr int kMaxDocumentLength = 200000;
  static constexpr int kMaxNeedleLength = 20000;

  static constexpr int kMaxFuzzyCandidates = 16;
  static constexpr double kMaxEditDistanceFraction = 0.15;
};