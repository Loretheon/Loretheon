#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QString>
#include <QStringList>

// Tracks which document scopes are included in the LLM context, and
// whether their content has drifted since the last time they were sent.
//
// Scopes are keyed by their stable ID (e.g. "md:section:intro#a3f2c1").
// Staleness is detected by comparing the scope's current content hash
// against the hash recorded at the last successful send.
class ContextModel : public QObject {
  Q_OBJECT

public:
  struct Entry {
    QString scopeId;
    QString heading;      // Human-readable label ("## Getting Started").
    QString contentHash;  // Current hash, refreshed on document change.
    QString sentHash;     // Hash at last send, empty if never sent.
    bool included = false;
    int depth = 0;        // 0 = root, 1 = top-level section, etc.

    bool isSent() const { return !sentHash.isEmpty(); }

    bool isStale() const {
      return isSent() && !contentHash.isEmpty() && contentHash != sentHash;
    }

    bool isNew() const { return !contentHash.isEmpty() && !isSent(); }
  };

  explicit ContextModel(QObject *parent = nullptr);

  void clear();

  // Replace the entire scope set. Preserves inclusion flags and sent
  // hashes for scopes whose IDs are unchanged; drops entries that are no
  // longer present. Emits changed().
  void setScopes(const QVector<Entry> &scopes);

  // Refresh only the content hashes and headings, keeping inclusion and
  // sentHash intact. Emits changed() if anything actually changed.
  void refreshContentHashes(const QHash<QString, QString> &newHashes,
                            const QHash<QString, QString> &newHeadings);

  // Mark the given scopes as included/excluded. Emits changed().
  void setIncluded(const QString &scopeId, bool included);
  void setAllIncluded(bool included);

  // Record the current content hashes as the sent hashes for the given
  // scopes. Called after a successful prompt submission or after applied
  // edits have authored new content. Emits changed() if anything changed.
  void markSent(const QStringList &scopeIds);

  // Returns the scope IDs selected for inclusion, in their original
  // order. Does not include the root unless explicitly included.
  QStringList includedScopeIds() const;

  // Returns the scope IDs whose content has drifted since last send.
  QStringList staleScopeIds() const;

  // Returns the full outline (all scopes) as a formatted string suitable
  // for the "Scope hierarchy" section of the planner prompt.
  QString outlineForModel() const;

  const QVector<Entry> &entries() const { return m_entries; }

  bool isEmpty() const { return m_entries.isEmpty(); }

signals:
  void changed();

private:
  Entry *findEntry(const QString &scopeId);

  QVector<Entry> m_entries;
};