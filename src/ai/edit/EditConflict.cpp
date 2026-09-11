// EditConflictDetector.cpp
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

void unite(QVector<int> &parent, int a, int b) {
  const int rootA = findRoot(parent, a);

  const int rootB = findRoot(parent, b);

  if (rootA != rootB) {
    parent[rootA] = rootB;
  }
}

} // namespace

namespace EditConflictDetector {

QVector<EditConflictGroup> detect(const QVector<ResolvedEdit> &edits) {
  QVector<EditConflictGroup> groups;

  const int count = edits.size();

  if (count < 2) {
    return groups;
  }

  QVector<int> parent(count);

  for (int i = 0; i < count; ++i) {
    parent[i] = i;
  }

  bool anyOverlap = false;

  for (int i = 0; i < count; ++i) {
    if (!edits.at(i).isValid()) {
      continue;
    }

    for (int j = i + 1; j < count; ++j) {
      if (!edits.at(j).isValid()) {
        continue;
      }

      if (rangesOverlap(edits.at(i).match, edits.at(j).match)) {

        unite(parent, i, j);

        anyOverlap = true;
      }
    }
  }

  if (!anyOverlap) {
    return groups;
  }

  QHash<int, int> rootToGroupIndex;

  for (int i = 0; i < count; ++i) {
    const int root = findRoot(parent, i);

    auto it = rootToGroupIndex.constFind(root);

    int groupIndex;

    if (it == rootToGroupIndex.constEnd()) {
      EditConflictGroup group;

      group.groupId = groups.size();

      groupIndex = groups.size();

      groups.append(group);

      rootToGroupIndex.insert(root, groupIndex);

    } else {
      groupIndex = it.value();
    }

    groups[groupIndex].planIndices.append(edits.at(i).planIndex);
  }

  QVector<EditConflictGroup> realGroups;

  for (const EditConflictGroup &group : groups) {
    if (group.planIndices.size() > 1) {
      realGroups.append(group);
    }
  }

  for (int i = 0; i < realGroups.size(); ++i) {
    realGroups[i].groupId = i;
  }

  return realGroups;
}

} // namespace EditConflictDetector