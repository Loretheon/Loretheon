#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

// A single Overseer session on disk. Layout:
//
//   Overseer/
//     memory.md                        (global, shared across sessions)
//     Sessions/
//       <name>/
//         transcript.md
//         overview.md
//         output/
//
// The session owns its folder and exposes read/write helpers for each of
// those files. The session is intentionally thin: it does not talk to the
// LLM, it does not own a widget. It is a filesystem-backed data object.

class OverseerSession : public QObject {
  Q_OBJECT

public:
  // Default constructor required by moc for QMetaType registration.
  // Produces an invalid session. Use create() or open() to get a usable one.
  explicit OverseerSession(QObject *parent = nullptr);

  // Construct a session object from an existing folder. The folder must
  // already exist and contain at least a transcript.md. Returns a session
  // whose isValid() is true, or an invalid one.
  static OverseerSession *open(const QString &rootPath, const QString &name,
                               QObject *parent = nullptr);

  // Create a new session. If a session with the same name already exists,
  // returns nullptr. The caller is responsible for handling the nullptr.
  static OverseerSession *create(const QString &rootPath, const QString &name,
                                 QObject *parent = nullptr);

  // Enumerate existing session names, sorted lexicographically.
  static QStringList list(const QString &rootPath);

  bool isValid() const { return m_valid; }

  QString name() const { return m_name; }
  QString folderPath() const { return m_folderPath; }
  QString outputPath() const { return m_outputPath; }
  QString transcriptPath() const { return m_transcriptPath; }
  QString overviewPath() const { return m_overviewPath; }

  // File contents. Empty string on failure.
  QString transcript() const;
  QString overview() const;

  // Writes. Return true on success.
  bool writeTranscript(const QString &text);
  bool writeOverview(const QString &text);

  // Append a message to the transcript. The format is:
  //
  //   ## <role> — <timestamp>
  //   <text>
  //
  // The file is created if it does not exist.
  bool appendTranscriptMessage(const QString &role, const QString &text);

signals:
  void changed();

private:
  OverseerSession(const QString &rootPath, const QString &name,
                  QObject *parent);

  bool ensureFolder();

  QString m_rootPath;
  QString m_name;
  QString m_folderPath;
  QString m_outputPath;
  QString m_transcriptPath;
  QString m_overviewPath;

  bool m_valid = false;
};