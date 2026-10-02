#pragma once

#include "ChatNode.h"

#include <QDate>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

class ChatTree;

// Persists one ChatTree segment to disk as a single markdown-ish
// file. Uses the same block format as TranscriptStore:
//
//   ## <kind> | <ISO8601 with ms> | <json>
//   <text>
//   <0x1E>
//
// The json sidecar carries the node's id, parentId, state, detail,
// jobId, result, error, and children ids. The text is the body.
// Order in the file is insertion order; children are reconstructed
// from parentId, not from position.
//
// A segment is one file. Nothing in this class creates a file per
// message or per day boundary. A new file is created only when the
// caller asks for a new segment.
class ChatTreeStore : public QObject {
  Q_OBJECT

public:
  explicit ChatTreeStore(QObject *parent = nullptr);

  // The assistant root, e.g. <assistantRoot>. Segment files go under
  // <root>/chats/<YYYY-MM-DD>/<name>.md.
  void setRoot(const QString &root);
  QString root() const { return m_root; }

  // Directory holding the segments for a given creation date.
  QString dayDirectory(const QDate &date) const;

  // Absolute path of the current segment file, or empty if none.
  QString currentPath() const { return m_currentPath; }
  QString currentName() const { return m_currentName; }
  QDate currentDate() const { return m_currentDate; }

  // Start a new segment. Generates a random name, sets the current
  // date to today, and returns the absolute path. The file is created
  // immediately so the segment is enumerable before any node exists.
  QString beginNewSegment();

  // Point the store at an existing segment file. Returns false if the
  // file does not exist.
  bool openSegment(const QString &absolutePath);

  // Load the current segment into `tree`. Clears the tree first.
  // Returns false on I/O failure. An empty or missing current path
  // leaves the tree empty and returns true.
  bool load(ChatTree *tree);

  // Write the whole tree to the current segment file, truncating.
  bool save(const ChatTree *tree);

  // Rename the file at `absolutePath` to `newName`. The file stays in
  // its existing day directory. `newName` is normalised to a
  // filesystem-safe base name with no extension. Returns the new
  // absolute path, or empty on failure. If `absolutePath` is the
  // current segment, the current path and name are updated.
  QString renameSegment(const QString &absolutePath,
                        const QString &newName);

  // Delete the file at `absolutePath`. Returns false on I/O failure.
  // If it was the current segment, the current path is cleared.
  bool deleteSegment(const QString &absolutePath);

  // A filesystem-safe base name for a user-supplied chat name. Empty
  // input becomes "chat". Collisions are not resolved here.
  static QString normaliseName(const QString &raw);

  // Enumerate every segment under the root, newest first. Each entry
  // carries the absolute path, the date, and the name.
  struct Entry {
    QString absolutePath;
    QDate date;
    QString name;
  };

  QVector<Entry> listSegments() const;

signals:
  void segmentChanged(const QString &absolutePath);

private:
  QString generateName() const;
  QString uniquePathIn(const QString &directory,
                       const QString &baseName,
                       const QString &ignorePath) const;

  QString m_root;
  QString m_currentPath;
  QString m_currentName;
  QDate m_currentDate;
};