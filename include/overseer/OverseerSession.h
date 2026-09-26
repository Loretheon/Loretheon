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
//         memory.md                    (session-scoped facts)
//         settings.json                (per-session toggles)
//         logs/                        (per-session conductor/agent/edit logs)
//         output/
//
// The session owns its folder and exposes read/write helpers for each
// of those files. The session is intentionally thin: it does not talk to
// the LLM, it does not own a widget. It is a filesystem-backed data
// object.

class OverseerSession : public QObject {
  Q_OBJECT

public:
  explicit OverseerSession(QObject *parent = nullptr);

  static OverseerSession *open(const QString &rootPath, const QString &name,
                               QObject *parent = nullptr);

  static OverseerSession *create(const QString &rootPath, const QString &name,
                                 QObject *parent = nullptr);

  static QStringList list(const QString &rootPath);

  bool isValid() const { return m_valid; }

  QString name() const { return m_name; }
  QString folderPath() const { return m_folderPath; }
  QString outputPath() const { return m_outputPath; }
  QString transcriptPath() const { return m_transcriptPath; }
  QString overviewPath() const { return m_overviewPath; }
  QString memoryPath() const { return m_memoryPath; }
  QString settingsPath() const { return m_settingsPath; }

  // Per-session log folder for the conductor, file agents and scoped
  // edit pipeline. Created on demand by the caller; not guaranteed to
  // exist until something writes to it.
  QString logsPath() const { return m_logsPath; }

  QString transcript() const;
  QString overview() const;
  QString memory() const;

  bool writeTranscript(const QString &text);
  bool writeOverview(const QString &text);
  bool writeMemory(const QString &text);

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
  QString m_memoryPath;
  QString m_settingsPath;
  QString m_logsPath;

  bool m_valid = false;
};