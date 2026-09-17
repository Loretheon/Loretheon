// EditConflict.h
#pragma once

#include "edit/EditCommand.h"
#include "edit/EditMatch.h"

#include <QVector>

/*
 * Represents one edit that has been resolved to a concrete
 * match, prior to content generation. Conflict detection
 * operates on these before any streaming begins.
 */
struct ResolvedEdit {
  int planIndex = -1;
  EditCommand command;
  EditMatch match;

  bool isValid() const {
    return planIndex >= 0 && command.isCommandValid() && match.isValid();
  }
};

/*
 * A group of resolved edits whose match ranges overlap
 * (directly or transitively). Groups with a single member
 * are not conflicts and are not constructed by the detector.
 */
struct EditConflictGroup {
  int groupId = -1;
  QVector<int> planIndices;
};

namespace EditConflictDetector {

/*
 * Pairwise range-overlap test matching EditApplier's actual
 * apply-time semantics: two ranges conflict if they intersect
 * at all, including a zero-length insertion point landing
 * inside another edit's [start, end) span.
 */
inline bool rangesOverlap(const EditMatch &a, const EditMatch &b) {
  return a.start < b.end && b.start < a.end;
}

/*
 * Groups resolved edits into conflict sets using transitive
 * closure over pairwise overlap (union-find). Edits with no
 * overlap against any other edit are omitted from the result.
 */
QVector<EditConflictGroup> detect(const QVector<ResolvedEdit> &edits);

} // namespace EditConflictDetector