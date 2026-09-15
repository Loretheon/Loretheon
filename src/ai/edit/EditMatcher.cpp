#include "EditMatcher.h"

#include <edlib.h>

#include <QSet>

namespace {

constexpr double MaxEditDistanceFraction = 0.20;
constexpr int MaxFuzzyCandidates = 16;

QVector<EditMatch> deduplicateCandidates(const QVector<EditMatch> &candidates) {
  QVector<EditMatch> result;
  result.reserve(candidates.size());

  QSet<QString> seen;

  for (const EditMatch &candidate : candidates) {
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

bool isValidScope(const DocumentNode *scope, const QString &documentText) {
  if (!scope || !scope->isValid()) {
    return false;
  }

  return scope->start >= 0 && scope->end >= scope->start &&
         scope->end <= documentText.size();
}

bool hasUsableResult(const EdlibAlignResult &result) {
  return result.status == EDLIB_STATUS_OK && result.editDistance >= 0 &&
         result.numLocations > 0 && result.startLocations != nullptr &&
         result.endLocations != nullptr;
}

bool isAsciiSafe(const QString &text) {
  for (const QChar character : text) {
    if (character.unicode() > 127) {
      return false;
    }
  }

  return true;
}

} // namespace

EditMatcher::Result EditMatcher::find(const TextDocument &document,
                                      const EditCommand &command) const {
  if (!command.isCommandValid()) {
    return {};
  }

  document.rebuildStructure();

  const DocumentStructure &structure = document.structure();

  const QString documentText = document.toPlainText();

  const DocumentNode *scope = structure.find(command.scopeId);

  if (!isValidScope(scope, documentText)) {
    return {};
  }

  // ReplaceScope is handled before the general guard, because its find
  // string is intentionally empty and the guard below would reject it.
  if (command.operation == EditCommand::Operation::ReplaceScope) {
    return findWholeScopeBody(document, command);
  }

  if (scope->start == scope->end ||
      command.operation == EditCommand::Operation::Insert ||
      command.findString.isEmpty()) {
    return {};
  }

  const QString scopeText =
      documentText.mid(scope->start, scope->end - scope->start);

  if (scopeText.isEmpty()) {
    return {};
  }

  const Result exactResult =
      findExact(scopeText, command.findString, scope->start);

  if (!exactResult.candidates.isEmpty()) {
    return exactResult;
  }

  return findFuzzy(scopeText, command.findString, scope->start);
}

EditMatcher::Result
EditMatcher::findWholeScopeBody(const TextDocument &document,
                                const EditCommand &command) const {
  Result result;

  if (!command.findString.isEmpty()) {
    return result;
  }

  if (command.position != EditCommand::Position::Inside) {
    return result;
  }

  document.rebuildStructure();

  const DocumentStructure &structure = document.structure();

  const QString documentText = document.toPlainText();

  const DocumentNode *scope = structure.find(command.scopeId);

  if (!isValidScope(scope, documentText)) {
    return result;
  }

  // Determine the body range. For Markdown sections, the body starts after
  // the heading line. For other node types, the whole node is the body.
  //
  // The root scope "document" always has a body equal to the whole file,
  // with no heading to skip.
  int bodyStart = scope->start;
  int bodyEnd = scope->end;

  const bool isRootScope = scope->id == QStringLiteral("document");

  if (!isRootScope && scope->type == QStringLiteral("section")) {
    int headingStart = -1;
    int headingEnd = -1;

    if (structure.headingRange(command.scopeId, headingStart, headingEnd)) {
      int afterHeading = headingEnd;

      while (afterHeading < bodyEnd &&
             (documentText.at(afterHeading) == QLatin1Char('\n') ||
              documentText.at(afterHeading) == QLatin1Char('\r'))) {
        ++afterHeading;
      }

      bodyStart = afterHeading;
    }
  }

  if (bodyEnd < bodyStart) {
    bodyEnd = bodyStart;
  }

  EditMatch match;

  match.start = bodyStart;
  match.end = bodyEnd;
  match.editDistance = 0;
  match.matchedText = documentText.mid(bodyStart, bodyEnd - bodyStart);

  result.candidates.append(match);

  return result;
}

EditMatcher::Result EditMatcher::findExact(const QString &text,
                                           const QString &needle,
                                           int offset) const {
  Result result;

  if (text.isEmpty() || needle.isEmpty()) {
    return result;
  }

  int position = text.indexOf(needle);

  while (position >= 0) {
    EditMatch match;

    match.start = offset + position;
    match.end = match.start + needle.size();
    match.editDistance = 0;
    match.matchedText = needle;

    result.candidates.append(match);

    position = text.indexOf(needle, position + needle.size());
  }

  return result;
}

EditMatcher::Result EditMatcher::findFuzzy(const QString &text,
                                           const QString &needle,
                                           int offset) const {
  Result result;
  result.fuzzy = true;

  if (text.isEmpty() || needle.isEmpty() || !isAsciiSafe(text) ||
      !isAsciiSafe(needle)) {
    return {};
  }

  const QByteArray textBytes = text.toLatin1();

  const QByteArray needleBytes = needle.toLatin1();

  const int maxEditDistance =
      qMax(1, static_cast<int>(needle.size() * MaxEditDistanceFraction));

  const EdlibAlignConfig config = edlibNewAlignConfig(
      maxEditDistance, EDLIB_MODE_HW, EDLIB_TASK_LOC, nullptr, 0);

  const EdlibAlignResult alignment =
      edlibAlign(needleBytes.constData(), needleBytes.size(),
                 textBytes.constData(), textBytes.size(), config);

  if (!hasUsableResult(alignment)) {
    edlibFreeAlignResult(alignment);
    return {};
  }

  const int locationCount = qMin(alignment.numLocations, MaxFuzzyCandidates);

  result.candidates.reserve(locationCount);

  for (int index = 0; index < locationCount; ++index) {
    const int localStart = alignment.startLocations[index];

    const int localEnd = alignment.endLocations[index] + 1;

    if (localStart < 0 || localEnd <= localStart || localEnd > text.size()) {
      continue;
    }

    EditMatch match;

    match.start = offset + localStart;
    match.end = offset + localEnd;
    match.editDistance = alignment.editDistance;

    match.matchedText = text.mid(localStart, localEnd - localStart);

    result.candidates.append(match);
  }

  edlibFreeAlignResult(alignment);

  result.candidates = deduplicateCandidates(result.candidates);

  return result;
}