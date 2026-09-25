#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class ScopeIndex;

// Copies files the worker produced into the user's notes folder and
// indexes what it wrote. The copy is one-way: the session keeps its
// own copy, the notes folder gains one.
//
// The destination is notesRoot/<sessionName>/<relative path under the
// session's output folder>. Directories are walked recursively and
// every .md file is copied. Files that already exist at the
// destination are skipped, never overwritten.
class NotePromoter : public QObject {
  Q_OBJECT

public:
  explicit NotePromoter(ScopeIndex *index, QObject *parent = nullptr);

  struct Result {
    QStringList written;           // copied and indexed
    QStringList skipped;           // destination already existed
    QStringList copiedNotIndexed;  // copied but the index rejected them
    QStringList failed;            // could not be read or copied
    QString error;                 // non-empty when the promote failed
    bool ok() const { return error.isEmpty(); }
  };

  // Promote a file or a folder. sourcePath is an absolute path under
  // the session's output folder. sessionName is the session it came
  // from. notesRoot is the root of the user's notes folder.
  Result promote(const QString &sourcePath,
                 const QString &sessionName,
                 const QString &notesRoot);

  signals:
    void progress(int current, int total);

private:
  Result promoteFile(const QString &sourcePath,
                     const QString &destinationRoot,
                     const QString &sourceRoot);

  bool copyMarkdownFile(const QString &source,
                        const QString &destination,
                        bool *skipped);

  ScopeIndex *m_index = nullptr;
};