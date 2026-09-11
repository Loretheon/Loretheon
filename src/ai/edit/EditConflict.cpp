#include "EditConflict.h"

#include <QHash>

namespace {

int findRoot(QVector<int> &parent, int index) {
  while (parent[index] != index) {
    parent[index] = parent[parent[index]];
    index = parent[index];
  }

  return index;
}

void unite(QVector<int> &parent, int first, int second) {
  const int firstRoot = findRoot(parent, first);
  const int secondRoot = findRoot(parent, second);

  if (firstRoot == secondRoot) {
    return;
  }

  parent[firstRoot] = secondRoot;
}

} // namespace

namespace EditConflictDetector {

QVector<EditConflictGroup> detect(const QVector<ResolvedEdit> &edits) {
  if (edits.size() < 2) {
    return {};
  }

  QVector<int> parent(edits.size());

  for (int index = 0; index < parent.size(); ++index) {
    parent[index] = index;
  }

  for (int first = 0; first < edits.size(); ++first) {
    if (!edits.at(first).isValid()) {
      continue;
    }

    for (int second = first + 1; second < edits.size(); ++second) {
      if (!edits.at(second).isValid()) {
        continue;
      }

      if (rangesOverlap(edits.at(first).match, edits.at(second).match)) {
        unite(parent, first, second);
      }
    }
  }

  QHash<int, QVector<int>> groupsByRoot;

  for (int index = 0; index < edits.size(); ++index) {
    if (!edits.at(index).isValid()) {
      continue;
    }

    const int root = findRoot(parent, index);
    groupsByRoot[root].append(edits.at(index).planIndex);
  }

  QVector<EditConflictGroup> groups;
  groups.reserve(groupsByRoot.size());

  for (auto it = groupsByRoot.cbegin(); it != groupsByRoot.cend(); ++it) {
    if (it.value().size() < 2) {
      continue;
    }

    EditConflictGroup group;
    group.groupId = groups.size();
    group.planIndices = it.value();

    groups.append(group);
  }

  return groups;
}

} // namespace EditConflictDetector