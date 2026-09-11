#include "EditMatcher.h"

#include <edlib.h>

#include <QByteArray>
#include <QSet>

namespace {

QVector<EditMatch> deduplicateCandidates(const QVector<EditMatch> &input) {
  QVector<EditMatch> result;

  QSet<QString> seen;

  for (const EditMatch &candidate : input) {
    const QString key =
        QStringLiteral("%1:%2").arg(candidate.start).arg(candidate.end);

    if (seen.contains(key)) {
      continue;
    }

    seen.insert(key);

    result.append(candidate);
  }

  return result;
}

} // namespace

EditMatcher::Result EditMatcher::find(const TextDocument &document,
                                      const EditCommand &command) const {
  Result result;

  /*
   * This matcher is resolving the structural command.
   * newString is not available yet during the command phase.
   */
  if (!command.isCommandValid()) {
    return result;
  }

  document.rebuildStructure();

  const DocumentStructure &structure = document.structure();

  const DocumentNode *scope = structure.find(command.scopeId);

  if (!scope || !scope->isValid()) {
    return result;
  }

  const QString documentText = document.toPlainText();

  if (scope->start < 0 || scope->end > static_cast<int>(documentText.size()) ||
      scope->end <= scope->start) {
    return result;
  }

  const QString scopeText =
      documentText.mid(scope->start, scope->end - scope->start);

  if (scopeText.isEmpty()) {
    return result;
  }

  if (command.operation == EditCommand::Operation::Insert) {
    return result;
  }

  result = findExact(scopeText, command.findString, scope->start);

  if (!result.isEmpty()) {
    return result;
  }

  return findFuzzy(scopeText, command.findString, scope->start);
}

EditMatcher::Result EditMatcher::findExact(const QString &text,
                                           const QString &needle,
                                           int offset) const {
  Result result;

  if (needle.isEmpty()) {
    return result;
  }

  int position = 0;

  while ((position = text.indexOf(needle, position)) >= 0) {
    EditMatch match;

    match.start = offset + position;

    match.end = match.start + needle.size();

    match.editDistance = 0;

    match.matchedText = needle;

    result.candidates.append(std::move(match));

    position += needle.size();
  }

  return result;
}

EditMatcher::Result EditMatcher::findFuzzy(const QString &text,
                                           const QString &needle,
                                           int offset) const {
  Result result;

  result.fuzzy = true;

  if (text.isEmpty() || needle.isEmpty()) {
    return result;
  }

  if (!isAsciiSafe(text) || !isAsciiSafe(needle)) {
    return {};
  }

  const QByteArray textBytes = text.toLatin1();

  const QByteArray needleBytes = needle.toLatin1();

  constexpr double kMaxEditDistanceFraction = 0.20;

  const int maxEditDistance =
      qMax(1, static_cast<int>(needle.size() * kMaxEditDistanceFraction));

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

  const int locationCount = qMin(edlibResult.numLocations, 16);

  result.candidates.reserve(locationCount);

  for (int i = 0; i < locationCount; ++i) {
    const int localStart = edlibResult.startLocations[i];

    const int localEnd = edlibResult.endLocations[i] + 1;

    if (localStart < 0 || localEnd <= localStart || localEnd > text.size()) {
      continue;
    }

    EditMatch match;

    match.start = offset + localStart;

    match.end = offset + localEnd;

    match.editDistance = edlibResult.editDistance;

    match.matchedText = text.mid(localStart, localEnd - localStart);

    result.candidates.append(std::move(match));
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