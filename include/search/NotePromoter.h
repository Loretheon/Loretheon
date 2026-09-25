#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class ScopeIndex;

class NotePromoter : public QObject {
  Q_OBJECT

public:
  explicit NotePromoter(ScopeIndex *index, QObject *parent = nullptr);

  struct Result {
    QStringList written;
    QStringList skipped;
    QStringList copiedNotIndexed;
    QStringList failed;
    QString error;
    bool ok() const { return error.isEmpty(); }
  };

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