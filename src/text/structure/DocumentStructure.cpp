#include "DocumentStructure.h"

#include <utility>

namespace {

QString previewText(const QString &text, int start, int end,
                    int maximumCharacters) {
  if (start < 0 || end < start || start >= text.size() ||
      maximumCharacters <= 0) {
    return {};
  }

  const int safeEnd = qBound(start, end, text.size());

  QString preview = text.mid(start, safeEnd - start).simplified();

  if (preview.size() <= maximumCharacters) {
    return preview;
  }

  if (maximumCharacters <= 3) {
    return preview.left(maximumCharacters);
  }

  return preview.left(maximumCharacters - 3) + QStringLiteral("...");
}

bool hasValidRange(const DocumentNode &node, int textLength) {
  return node.start >= 0 && node.end >= node.start && node.end <= textLength &&
         node.end > node.start;
}

} // namespace

void DocumentStructure::clear() {
  m_text.clear();
  m_root = DocumentNode();
}

void DocumentStructure::setText(const QString &text) { m_text = text; }

void DocumentStructure::setRoot(DocumentNode root) { m_root = std::move(root); }

const QString &DocumentStructure::text() const { return m_text; }

const DocumentNode &DocumentStructure::root() const { return m_root; }

const DocumentNode *DocumentStructure::find(const QString &id) const {
  if (id.isEmpty()) {
    return nullptr;
  }

  return findRecursive(m_root, id);
}

const DocumentNode *DocumentStructure::findRecursive(const DocumentNode &node,
                                                     const QString &id) const {
  if (node.id == id) {
    return &node;
  }

  for (const DocumentNode &child : node.children) {
    if (const DocumentNode *result = findRecursive(child, id)) {
      return result;
    }
  }

  return nullptr;
}

QString DocumentStructure::contentHashFor(const QString &scopeId) const {
  const DocumentNode *node = find(scopeId);

  return node ? node->contentHash : QString();
}

bool DocumentStructure::headingRange(const QString &scopeId, int &start,
                                     int &end) const {
  start = -1;
  end = -1;

  const DocumentNode *node = find(scopeId);

  if (!node || node->type != QStringLiteral("section")) {
    return false;
  }

  const int scopeStart = qBound(0, node->start, m_text.size());
  const int scopeEnd = qBound(scopeStart, node->end, m_text.size());

  if (scopeEnd <= scopeStart) {
    return false;
  }

  int cursor = scopeStart;

  while (cursor < scopeEnd) {
    int lineEnd = m_text.indexOf(QChar('\n'), cursor);

    if (lineEnd < 0 || lineEnd > scopeEnd) {
      lineEnd = scopeEnd;
    }

    const QString line = m_text.mid(cursor, lineEnd - cursor).trimmed();

    if (line.startsWith(QChar('#'))) {
      start = cursor;
      end = lineEnd;
      return true;
    }

    cursor = lineEnd + 1;
  }

  return false;
}

QStringList DocumentStructure::scopeIds() const {
  QStringList ids;

  if (m_root.id.isEmpty()) {
    return ids;
  }

  // Always expose the root so the planner can reference the whole
  // document when no sections exist. Sections are appended afterwards;
  // duplicates are avoided by the contains() check.
  ids.append(m_root.id);

  appendScopeIdsRecursive(m_root, ids);
  return ids;
}

void DocumentStructure::appendScopeIdsRecursive(const DocumentNode &node,
                                                QStringList &ids) const {
  if (node.type == QStringLiteral("section") && !node.id.isEmpty() &&
      !ids.contains(node.id)) {
    ids.append(node.id);
  }

  for (const DocumentNode &child : node.children) {
    appendScopeIdsRecursive(child, ids);
  }
}

QString
DocumentStructure::sectionIndexForModel(int maxPreviewCharacters) const {
  if (m_root.id.isEmpty()) {
    return {};
  }

  QString output;
  appendSectionIndexRecursive(m_root, output, 0, maxPreviewCharacters);

  return output;
}

void DocumentStructure::appendSectionIndexRecursive(
    const DocumentNode &node, QString &output, int depth,
    int maxPreviewCharacters) const {
  if (node.type == QStringLiteral("section")) {
    output += QString(depth * 2, QChar(' '));
    output += node.id;
    output += QStringLiteral(" [section]");

    if (hasValidRange(node, m_text.size())) {
      const QString preview =
          previewText(m_text, node.start, node.end, maxPreviewCharacters);

      if (!preview.isEmpty()) {
        output += QStringLiteral(" \"%1\"").arg(preview);
      }
    }

    output += QChar('\n');
    ++depth;
  }

  for (const DocumentNode &child : node.children) {
    appendSectionIndexRecursive(child, output, depth, maxPreviewCharacters);
  }
}

QString DocumentStructure::indexForModel(int maxPreviewCharacters) const {
  if (m_root.id.isEmpty()) {
    return {};
  }

  QString output;
  appendIndexRecursive(m_root, output, 0, maxPreviewCharacters);

  return output;
}

void DocumentStructure::appendIndexRecursive(const DocumentNode &node,
                                             QString &output, int depth,
                                             int maxPreviewCharacters) const {
  output += QString(depth * 2, QChar(' '));
  output += node.id;
  output += QStringLiteral(" [%1]").arg(node.type);

  if (hasValidRange(node, m_text.size())) {
    const QString preview =
        previewText(m_text, node.start, node.end, maxPreviewCharacters);

    if (!preview.isEmpty()) {
      output += QStringLiteral(" \"%1\"").arg(preview);
    }
  }

  output += QChar('\n');

  for (const DocumentNode &child : node.children) {
    appendIndexRecursive(child, output, depth + 1, maxPreviewCharacters);
  }
}