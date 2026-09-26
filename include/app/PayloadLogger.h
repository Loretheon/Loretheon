#pragma once

#include <QFile>
#include <QHash>
#include <QObject>
#include <QString>
#include <QTextStream>

// A payload logger. Writes one file per subsystem into a run folder,
// so that the conductor, the file agents, the scoped edit pipeline,
// and the chat each write to their own log without interleaving.
//
// Subsystems are identified by a short string: "conductor", "agent",
// "edit", "chat", "render", "app", "inference". Use the enum or the
// string.
//
// Two constructors:
//   - PayloadLogger(QObject*) creates a fresh run folder under the
//     app's log directory named for the current run.
//   - PayloadLogger(const QString &folder, QObject*) writes into a
//     folder supplied by the caller. Used for session-scoped loggers,
//     where the folder is the session's logs/ directory.
//
// Every log entry is timestamped with ISO 8601 milliseconds, tagged,
// and written with a blank line between entries.
class PayloadLogger : public QObject {
  Q_OBJECT

public:
  enum class Subsystem {
    App,
    Conductor,
    Agent,
    Edit,
    Chat,
    Render,
    Inference,
  };

  explicit PayloadLogger(QObject *parent = nullptr);

  // Create a logger that writes into an explicit folder. The folder is
  // created if it does not exist. Used for session-scoped loggers.
  explicit PayloadLogger(const QString &folder, QObject *parent = nullptr);

  // Create a second logger that shares the same run folder as an
  // existing logger.
  PayloadLogger(PayloadLogger *parentLogger, QObject *parent = nullptr);

  ~PayloadLogger() override;

  void log(Subsystem subsystem, const QString &tag,
           const QString &content);

  void log(const QString &subsystem, const QString &tag,
           const QString &content);

  void log(const QString &tag, const QString &content);

  QString runFolderPath() const { return m_runFolder; }

  QString filePathFor(Subsystem subsystem) const;
  QString filePathFor(const QString &subsystem) const;

  void inheritRunFolder(PayloadLogger *parentLogger);

private:
  static QString subsystemName(Subsystem subsystem);

  void ensureOpen(Subsystem subsystem);
  void ensureOpen(const QString &subsystem);

  QString m_runFolder;

  QHash<QString, QFile *> m_files;
  QHash<QString, QTextStream *> m_streams;

  QFile *m_fallbackFile = nullptr;
  QTextStream *m_fallbackStream = nullptr;
};