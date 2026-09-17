#include "../../../include/ai/context/ContextModel.h"

#include <algorithm>

ContextModel::ContextModel(QObject *parent) : QObject(parent) {}

void ContextModel::clear() {
  if (m_entries.isEmpty()) {
    return;
  }

  m_entries.clear();
  emit changed();
}

void ContextModel::setScopes(const QVector<Entry> &scopes) {
  QHash<QString, Entry> previous;

  for (const Entry &entry : std::as_const(m_entries)) {
    previous.insert(entry.scopeId, entry);
  }

  QVector<Entry> merged;
  merged.reserve(scopes.size());

  for (const Entry &incoming : scopes) {
    Entry entry = incoming;

    auto it = previous.constFind(entry.scopeId);

    if (it != previous.constEnd()) {
      entry.included = it->included;
      entry.sentHash = it->sentHash;
    }

    merged.append(entry);
  }

  m_entries = std::move(merged);

  emit changed();
}

void ContextModel::refreshContentHashes(
    const QHash<QString, QString> &newHashes,
    const QHash<QString, QString> &newHeadings) {
  bool anyChange = false;

  for (Entry &entry : m_entries) {
    const QString newHash = newHashes.value(entry.scopeId);
    const QString newHeading = newHeadings.value(entry.scopeId);

    if (entry.contentHash != newHash) {
      entry.contentHash = newHash;
      anyChange = true;
    }

    if (!newHeading.isEmpty() && entry.heading != newHeading) {
      entry.heading = newHeading;
      anyChange = true;
    }
  }

  if (anyChange) {
    emit changed();
  }
}

ContextModel::Entry *ContextModel::findEntry(const QString &scopeId) {
  for (Entry &entry : m_entries) {
    if (entry.scopeId == scopeId) {
      return &entry;
    }
  }

  return nullptr;
}

void ContextModel::setIncluded(const QString &scopeId, bool included) {
  Entry *entry = findEntry(scopeId);

  if (!entry || entry->included == included) {
    return;
  }

  entry->included = included;

  emit changed();
}

void ContextModel::setAllIncluded(bool included) {
  bool anyChange = false;

  for (Entry &entry : m_entries) {
    if (entry.included != included) {
      entry.included = included;
      anyChange = true;
    }
  }

  if (anyChange) {
    emit changed();
  }
}

void ContextModel::markSent(const QStringList &scopeIds) {
  bool anyChange = false;

  for (Entry &entry : m_entries) {
    if (!scopeIds.contains(entry.scopeId)) {
      continue;
    }

    if (entry.sentHash != entry.contentHash) {
      entry.sentHash = entry.contentHash;
      anyChange = true;
    }
  }

  if (anyChange) {
    emit changed();
  }
}

QStringList ContextModel::includedScopeIds() const {
  QStringList result;

  for (const Entry &entry : m_entries) {
    if (entry.included) {
      result.append(entry.scopeId);
    }
  }

  return result;
}

QStringList ContextModel::staleScopeIds() const {
  QStringList result;

  for (const Entry &entry : m_entries) {
    if (entry.isStale()) {
      result.append(entry.scopeId);
    }
  }

  return result;
}

QString ContextModel::outlineForModel() const {
  QStringList lines;

  for (const Entry &entry : m_entries) {
    QString marker = QStringLiteral("  ");

    if (entry.isStale()) {
      marker = QStringLiteral("* ");
    } else if (entry.isNew()) {
      marker = QStringLiteral("+ ");
    }

    const QString indent(entry.depth * 2, QChar(' '));

    const QString label =
        entry.heading.isEmpty() ? entry.scopeId : entry.heading;

    lines.append(QStringLiteral("%1%2%3 [%4]")
                     .arg(marker, indent, label, entry.scopeId));
  }

  return lines.join('\n');
}