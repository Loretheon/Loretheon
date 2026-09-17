#pragma once

#include <QString>
#include <QVector>

struct DocumentNode {
  QString id;
  QString type;

  int start = -1;
  int end = -1;

  // SHA-1 (12 hex chars) of the node's own source text. Used by the token
  // layer to detect drift without reparsing.
  QString contentHash;

  QVector<DocumentNode> children;

  bool isRoot() const { return id == QStringLiteral("document"); }

  bool isValid() const {
    if (isRoot()) {
      return start >= 0 && end >= start;
    }

    return !id.isEmpty() && start >= 0 && end > start;
  }
};