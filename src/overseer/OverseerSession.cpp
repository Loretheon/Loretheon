#include "OverseerSession.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>

namespace {

constexpr auto MemoryFilename = "memory.md";
constexpr auto TranscriptFilename = "transcript.md";
constexpr auto OverviewFilename = "overview.md";
constexpr auto OutputDirname = "output";

QString sessionsRoot(const QString &rootPath) {
  return QDir(rootPath).filePath(QStringLiteral("Sessions"));
}

bool isSafeSessionName(const QString &name) {
  if (name.isEmpty() || name == QStringLiteral(".") ||
      name == QStringLiteral("..")) {
    return false;
  }

  static const QRegularExpression disallowed(
      QStringLiteral("[\\\\/:*?\"<>|\\x00-\\x1F]"));

  return !name.contains(disallowed);
}

QString readTextFile(const QString &path) {
  QFile file(path);

  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return {};
  }

  QTextStream stream(&file);
  stream.setEncoding(QStringConverter::Utf8);

  const QString text = stream.readAll();

  if (stream.status() != QTextStream::Ok) {
    return {};
  }

  return text;
}

bool writeTextFile(const QString &path, const QString &text) {
  QFile file(path);

  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate |
                 QIODevice::Text)) {
    return false;
  }

  QTextStream stream(&file);
  stream.setEncoding(QStringConverter::Utf8);
  stream << text;

  if (stream.status() != QTextStream::Ok) {
    return false;
  }

  return true;
}

} // namespace
OverseerSession::OverseerSession(QObject *parent) : QObject(parent) {}
OverseerSession::OverseerSession(const QString &rootPath, const QString &name,
                                 QObject *parent)
    : QObject(parent), m_rootPath(rootPath), m_name(name) {}

OverseerSession *OverseerSession::open(const QString &rootPath,
                                       const QString &name, QObject *parent) {
  if (!isSafeSessionName(name)) {
    return nullptr;
  }

  auto *session = new OverseerSession(rootPath, name, parent);

  session->m_folderPath =
      QDir(sessionsRoot(rootPath)).filePath(name);
  session->m_outputPath =
      QDir(session->m_folderPath).filePath(QString::fromLatin1(OutputDirname));
  session->m_transcriptPath =
      QDir(session->m_folderPath).filePath(QString::fromLatin1(TranscriptFilename));
  session->m_overviewPath =
      QDir(session->m_folderPath).filePath(QString::fromLatin1(OverviewFilename));

  if (!QDir(session->m_folderPath).exists() ||
      !QFileInfo::exists(session->m_transcriptPath)) {
    delete session;
    return nullptr;
  }

  session->m_valid = true;
  return session;
}

OverseerSession *OverseerSession::create(const QString &rootPath,
                                         const QString &name,
                                         QObject *parent) {
  if (!isSafeSessionName(name)) {
    return nullptr;
  }

  auto *session = new OverseerSession(rootPath, name, parent);

  session->m_folderPath =
      QDir(sessionsRoot(rootPath)).filePath(name);
  session->m_outputPath =
      QDir(session->m_folderPath).filePath(QString::fromLatin1(OutputDirname));
  session->m_transcriptPath =
      QDir(session->m_folderPath).filePath(QString::fromLatin1(TranscriptFilename));
  session->m_overviewPath =
      QDir(session->m_folderPath).filePath(QString::fromLatin1(OverviewFilename));

  if (QFileInfo::exists(session->m_folderPath)) {
    delete session;
    return nullptr;
  }

  if (!session->ensureFolder()) {
    delete session;
    return nullptr;
  }

  session->m_valid = true;
  return session;
}

QStringList OverseerSession::list(const QString &rootPath) {
  QDir dir(sessionsRoot(rootPath));

  if (!dir.exists()) {
    return {};
  }

  const QStringList entries =
      dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);

  QStringList result;

  for (const QString &entry : entries) {
    const QString transcriptPath =
        dir.filePath(QDir(entry).filePath(
            QString::fromLatin1(TranscriptFilename)));

    if (QFileInfo::exists(transcriptPath)) {
      result.append(entry);
    }
  }

  return result;
}

bool OverseerSession::ensureFolder() {
  QDir dir;

  if (!dir.mkpath(m_folderPath)) {
    return false;
  }

  if (!dir.mkpath(m_outputPath)) {
    return false;
  }

  if (!QFileInfo::exists(m_transcriptPath)) {
    if (!writeTextFile(m_transcriptPath, QString())) {
      return false;
    }
  }

  if (!QFileInfo::exists(m_overviewPath)) {
    const QString header = QStringLiteral("# Overview\n\n");

    if (!writeTextFile(m_overviewPath, header)) {
      return false;
    }
  }

  return true;
}

QString OverseerSession::transcript() const {
  return readTextFile(m_transcriptPath);
}

QString OverseerSession::overview() const {
  return readTextFile(m_overviewPath);
}

bool OverseerSession::writeTranscript(const QString &text) {
  if (!writeTextFile(m_transcriptPath, text)) {
    return false;
  }

  emit changed();
  return true;
}

bool OverseerSession::writeOverview(const QString &text) {
  if (!writeTextFile(m_overviewPath, text)) {
    return false;
  }

  emit changed();
  return true;
}

bool OverseerSession::appendTranscriptMessage(const QString &role,
                                              const QString &text) {
  QString existing = transcript();

  if (!existing.isEmpty() && !existing.endsWith(QChar('\n'))) {
    existing += QChar('\n');
  }

  const QString timestamp =
      QDateTime::currentDateTime().toString(Qt::ISODate);

  const QString entry = QStringLiteral("## %1 — %2\n%3\n\n")
                            .arg(role, timestamp, text);

  if (!writeTextFile(m_transcriptPath, existing + entry)) {
    return false;
  }

  emit changed();
  return true;
}