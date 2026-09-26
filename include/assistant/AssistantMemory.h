#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>
#include <QVector>

class InferenceService;
class MemoryIndex;

// The assistant's durable memory. Two storage shapes:
//
//   memories/sessions/YYYY-MM-DD-HHMM.md
//     One file per session. Written when the session ends. Contains a
//     short, human-readable summary of what happened.
//
//   memories/topics/<slug>.md
//     One file per topic, appended to across sessions. Facts about a
//     topic accumulate here permanently.
//
// Both directories are indexed by the semantic search. Recall is a
// search query, not a file read.
//
// Writing is gated: the caller proposes a fact, the user approves it,
// and only then does it land on disk. The propose/approve flow lives
// in the assistant conductor; this class only performs the write once
// approval has been given.
class AssistantMemory {
public:
  AssistantMemory() = default;

  void setRoot(const QString &root);
  QString root() const { return m_root; }

  void setMemoryIndex(MemoryIndex *index);
  void setInference(InferenceService *inference);

  // Ensure the memories/ tree exists. Called by load().
  bool ensureRoot() const;

  // Read every file under memories/ and return their paths. Used by
  // the scope index to know what to embed.
  QStringList memoryFiles() const;

  // Absolute path of a session file for the given timestamp.
  QString sessionPath(const QDateTime &timestamp) const;

  // Absolute path of a topic file for the given topic slug.
  QString topicPath(const QString &slug) const;

  // Write a fact into a topic file, creating the file if needed. The
  // fact is appended with a timestamp header. Returns false on I/O
  // failure.
  bool appendToTopic(const QString &slug, const QString &fact);

  // Write a session summary. The file is created fresh. Returns false
  // on I/O failure.
  bool writeSession(const QDateTime &timestamp, const QString &summary);

  // Semantic recall. Uses the search index if available. Returns the
  // bodies of the top-K memory hits. Empty when no search is set or
  // nothing matches. This is what makes the assistant remember without
  // being asked.
  QStringList recall(const QString &query, int k = 4) const;

  // Turn a topic name into a filesystem-safe slug.
  static QString slugify(const QString &topic);

  // Read a file's contents. Empty on failure.
  static QString readFile(const QString &path);

private:
  QString m_root;
  MemoryIndex *m_index = nullptr;
  InferenceService *m_inference = nullptr;
};