#include "EditMatcher.h"

#include <edlib.h>

#include <QByteArray>
#include <QSet>

namespace {

QVector<EditMatch> deduplicateCandidates(const QVector<EditMatch> &input) {
  QVector<EditMatch> result;
  QSet<QString> seen;

  for (const EditMatch &candidate : input) {
    const QString key = QString::number(candidate.start) + QLatin1Char(':') +
                        QString::number(candidate.end);

    if (seen.contains(key)) {
      continue;
    }

    seen.insert(key);
    result.append(candidate);
  }

  return result;
}

} // namespace

EditMatcher::Result EditMatcher::find(const QTextDocument &document,
                                      const EditCommand &command) const {
  if (!command.isValid()) {
    return {};
  }

  const QString text = document.toPlainText();

  if (text.isEmpty()) {
    return {};
  }

  /*
   * Exact matching always gets first refusal.
   *
   * This is important: fuzzy matching must never override an exact
   * occurrence.
   */
  const Result exact = findExact(text, command.oldString);

  if (!exact.isEmpty()) {
    return exact;
  }

  return findFuzzy(text, command.oldString);
}

EditMatcher::Result EditMatcher::findExact(const QString &text,
                                           const QString &needle) const {
  Result result;

  if (needle.isEmpty()) {
    return result;
  }

  int position = 0;

  while ((position = text.indexOf(needle, position)) >= 0) {
    EditMatch match;
    match.start = position;
    match.end = position + needle.length();
    match.editDistance = 0;

    result.candidates.append(match);

    position += needle.length();
  }

  return result;
}

EditMatcher::Result EditMatcher::findFuzzy(const QString &text,
                                           const QString &needle) const {
  Result result;
  result.fuzzy = true;

  /*
   * Hard input bounds.
   *
   * The matcher never receives arbitrarily large input.
   */
  if (text.length() > kMaxDocumentLength ||
      needle.length() > kMaxNeedleLength) {
    return {};
  }

  /*
   * Keep the current Edlib adapter deliberately conservative.
   * QString offsets are directly usable because ASCII means one byte
   * per QString character here.
   */
  if (!isAsciiSafe(text) || !isAsciiSafe(needle)) {
    return {};
  }

  const QByteArray textBytes = text.toLatin1();

  const QByteArray needleBytes = needle.toLatin1();

  const int maxEditDistance =
      qMax(1, static_cast<int>(needle.length() * kMaxEditDistanceFraction));

  const EdlibAlignConfig config = edlibNewAlignConfig(
      maxEditDistance, EDLIB_MODE_HW, EDLIB_TASK_LOC, nullptr, 0);

  const EdlibAlignResult edlibResult =
      edlibAlign(needleBytes.constData(), needleBytes.size(),
                 textBytes.constData(), textBytes.size(), config);

  if (edlibResult.status != EDLIB_STATUS_OK || edlibResult.editDistance < 0 ||
      edlibResult.numLocations <= 0 || edlibResult.startLocations == nullptr ||
      edlibResult.endLocations == nullptr) {
    edlibFreeAlignResult(edlibResult);
    return {};
  }

  const int locationCount = qMin(edlibResult.numLocations, kMaxFuzzyCandidates);

  result.candidates.reserve(locationCount);

  for (int i = 0; i < locationCount; ++i) {
    const int start = edlibResult.startLocations[i];

    const int end = edlibResult.endLocations[i] + 1;

    if (start < 0 || end <= start || end > text.length()) {
      continue;
    }

    EditMatch match;
    match.start = start;
    match.end = end;
    match.editDistance = edlibResult.editDistance;

    result.candidates.append(match);
  }

  edlibFreeAlignResult(edlibResult);

  result.candidates = deduplicateCandidates(result.candidates);

  return result;
}

bool EditMatcher::isAsciiSafe(const QString &text) {
  for (const QChar &character : text) {
    if (character.unicode() > 127) {
      return false;
    }
  }

  return true;
}