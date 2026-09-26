#include "PayloadLogger.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>

namespace {

QString timestamp() {
  return QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
}

QString makeRunFolderName() {
  return QDateTime::currentDateTime().toString(
      QStringLiteral("yyyyMMdd_hhmmss_zzz"));
}

QString normalizeSubsystemName(const QString &raw) {
  const QString lower = raw.trimmed().toLower();

  if (lower.isEmpty())
    return QStringLiteral("app");

  return lower;
}

} // namespace

PayloadLogger::PayloadLogger(QObject *parent) : QObject(parent) {
  const QString base =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
      QStringLiteral("/logs/payloads");

  QDir baseDir;

  if (!baseDir.mkpath(base)) {
    qWarning() << "[PayloadLogger] Failed to create base log directory:"
               << base;
    return;
  }

  const QString folderName = makeRunFolderName();

  m_runFolder = QDir(base).filePath(folderName);

  if (!baseDir.mkpath(m_runFolder)) {
    qWarning() << "[PayloadLogger] Failed to create run folder:"
               << m_runFolder;
    m_runFolder.clear();
    return;
  }

  qDebug() << "[PayloadLogger] Run folder:" << m_runFolder;
}

PayloadLogger::PayloadLogger(const QString &folder, QObject *parent)
    : QObject(parent) {
  if (folder.trimmed().isEmpty()) {
    qWarning() << "[PayloadLogger] Empty session log folder supplied.";
    return;
  }

  QDir dir;

  if (!dir.mkpath(folder)) {
    qWarning() << "[PayloadLogger] Failed to create log folder:" << folder;
    return;
  }

  m_runFolder = folder;

  qDebug() << "[PayloadLogger] Session log folder:" << m_runFolder;
}

PayloadLogger::PayloadLogger(PayloadLogger *parentLogger, QObject *parent)
    : QObject(parent) {
  if (!parentLogger) {
    qWarning() << "[PayloadLogger] Cannot construct child logger without "
                  "parent logger.";
    return;
  }

  m_runFolder = parentLogger->runFolderPath();

  if (m_runFolder.isEmpty()) {
    qWarning() << "[PayloadLogger] Parent logger has no run folder.";
  }
}

PayloadLogger::~PayloadLogger() {
  for (auto it = m_streams.begin(); it != m_streams.end(); ++it) {
    if (it.value()) {
      it.value()->flush();
      delete it.value();
    }
  }

  for (auto it = m_files.begin(); it != m_files.end(); ++it) {
    if (it.value()) {
      if (it.value()->isOpen())
        it.value()->close();
      delete it.value();
    }
  }

  m_streams.clear();
  m_files.clear();

  if (m_fallbackStream) {
    m_fallbackStream->flush();
    delete m_fallbackStream;
  }

  if (m_fallbackFile) {
    if (m_fallbackFile->isOpen())
      m_fallbackFile->close();
    delete m_fallbackFile;
  }
}

void PayloadLogger::inheritRunFolder(PayloadLogger *parentLogger) {
  if (!parentLogger)
    return;

  m_runFolder = parentLogger->runFolderPath();
}

QString PayloadLogger::subsystemName(Subsystem subsystem) {
  switch (subsystem) {
  case Subsystem::App:
    return QStringLiteral("app");
  case Subsystem::Conductor:
    return QStringLiteral("conductor");
  case Subsystem::Agent:
    return QStringLiteral("agent");
  case Subsystem::Edit:
    return QStringLiteral("edit");
  case Subsystem::Chat:
    return QStringLiteral("chat");
  case Subsystem::Render:
    return QStringLiteral("render");
  case Subsystem::Inference:
    return QStringLiteral("inference");
  }

  return QStringLiteral("app");
}

QString PayloadLogger::filePathFor(Subsystem subsystem) const {
  if (m_runFolder.isEmpty())
    return {};

  return QDir(m_runFolder)
      .filePath(subsystemName(subsystem) + QStringLiteral(".log"));
}

QString PayloadLogger::filePathFor(const QString &subsystem) const {
  if (m_runFolder.isEmpty())
    return {};

  return QDir(m_runFolder)
      .filePath(normalizeSubsystemName(subsystem) + QStringLiteral(".log"));
}

void PayloadLogger::ensureOpen(Subsystem subsystem) {
  ensureOpen(subsystemName(subsystem));
}

void PayloadLogger::ensureOpen(const QString &subsystem) {
  const QString name = normalizeSubsystemName(subsystem);

  if (m_files.contains(name))
    return;

  if (m_runFolder.isEmpty()) {
    if (!m_fallbackFile) {
      const QString path =
          QDir(QStandardPaths::writableLocation(
                   QStandardPaths::AppDataLocation) +
               QStringLiteral("/logs/payloads"))
              .filePath(QStringLiteral("_payload_fallback.log"));

      m_fallbackFile = new QFile(path);

      if (!m_fallbackFile->open(QIODevice::WriteOnly | QIODevice::Append |
                                QIODevice::Text)) {
        qWarning() << "[PayloadLogger] Failed to open fallback log file:"
                   << path;
        delete m_fallbackFile;
        m_fallbackFile = nullptr;
        return;
      }

      m_fallbackStream = new QTextStream(m_fallbackFile);
    }

    return;
  }

  const QString path =
      QDir(m_runFolder).filePath(name + QStringLiteral(".log"));

  auto *file = new QFile(path);

  if (!file->open(QIODevice::WriteOnly | QIODevice::Append |
                  QIODevice::Text)) {
    qWarning() << "[PayloadLogger] Failed to open subsystem log:" << path;
    delete file;
    return;
  }

  auto *stream = new QTextStream(file);
  stream->setEncoding(QStringConverter::Utf8);

  m_files.insert(name, file);
  m_streams.insert(name, stream);
}

void PayloadLogger::log(Subsystem subsystem, const QString &tag,
                        const QString &content) {
  log(subsystemName(subsystem), tag, content);
}

void PayloadLogger::log(const QString &subsystem, const QString &tag,
                        const QString &content) {
  const QString name = normalizeSubsystemName(subsystem);

  ensureOpen(name);

  QTextStream *stream = m_streams.value(name, nullptr);

  if (!stream)
    stream = m_fallbackStream;

  if (!stream)
    return;

  *stream << "[" << tag << "] " << timestamp() << "\n"
          << content << "\n\n";

  stream->flush();
}

void PayloadLogger::log(const QString &tag, const QString &content) {
  log(QStringLiteral("app"), tag, content);
}