#include "OverseerSession.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QTextStream>

namespace {

constexpr auto SessionsDirname = "Sessions";

bool isValidName(const QString &name) {
  if (name.isEmpty())
    return false;

  static const QRegularExpression re(QStringLiteral(R"(^[A-Za-z0-9 _\-]+$)"));

  return re.match(name).hasMatch();
}

} // namespace

OverseerSession::OverseerSession(QObject *parent) : QObject(parent) {
  m_valid = false;
}


OverseerSession::OverseerSession(const QString &rootPath, const QString &name,
                                 QObject *parent)
    : QObject(parent), m_rootPath(rootPath), m_name(name) {
  if (!isValidName(name))
    return;

  m_folderPath =
      QDir(QDir(rootPath).filePath(SessionsDirname)).filePath(name);

  m_outputPath = QDir(m_folderPath).filePath(QStringLiteral("output"));
  m_transcriptPath =
      QDir(m_folderPath).filePath(QStringLiteral("transcript.md"));
  m_overviewPath =
      QDir(m_folderPath).filePath(QStringLiteral("overview.md"));
  m_memoryPath = QDir(m_folderPath).filePath(QStringLiteral("memory.md"));
  m_settingsPath = QDir(m_folderPath).filePath(QStringLiteral("settings.json"));
  m_logsPath = QDir(m_folderPath).filePath(QStringLiteral("logs"));

  m_valid = QFileInfo::exists(m_folderPath);
}

OverseerSession *OverseerSession::open(const QString &rootPath,
                                       const QString &name,
                                       QObject *parent) {
  auto *session = new OverseerSession(rootPath, name, parent);

  if (!session->isValid()) {
    delete session;
    return nullptr;
  }

  return session;
}

OverseerSession *OverseerSession::create(const QString &rootPath,
                                         const QString &name,
                                         QObject *parent) {
  if (!isValidName(name))
    return nullptr;

  const QString folder =
      QDir(QDir(rootPath).filePath(SessionsDirname)).filePath(name);

  if (QFileInfo::exists(folder))
    return nullptr;

  QDir rootDir(rootPath);

  if (!rootDir.mkpath(QDir(folder).filePath(QStringLiteral("output"))))
    return nullptr;

  auto *session = new OverseerSession(rootPath, name, parent);

  if (!session->isValid()) {
    delete session;
    return nullptr;
  }

  session->ensureFolder();

  return session;
}

QStringList OverseerSession::list(const QString &rootPath) {
  QDir dir(QDir(rootPath).filePath(SessionsDirname));

  if (!dir.exists())
    return {};

  return dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
}

bool OverseerSession::ensureFolder() {
  QDir dir;

  if (!dir.mkpath(m_folderPath))
    return false;

  if (!dir.mkpath(m_outputPath))
    return false;

  if (!dir.mkpath(m_logsPath))
    return false;

  return true;
}

QString OverseerSession::transcript() const {
  QFile file(m_transcriptPath);

  if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    return {};

  return QString::fromUtf8(file.readAll());
}

QString OverseerSession::overview() const {
  QFile file(m_overviewPath);

  if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    return {};

  return QString::fromUtf8(file.readAll());
}

QString OverseerSession::memory() const {
  QFile file(m_memoryPath);

  if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    return {};

  return QString::fromUtf8(file.readAll());
}

bool OverseerSession::writeTranscript(const QString &text) {
  QFile file(m_transcriptPath);

  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate |
                 QIODevice::Text))
    return false;

  file.write(text.toUtf8());
  return true;
}

bool OverseerSession::writeOverview(const QString &text) {
  QFile file(m_overviewPath);

  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate |
                 QIODevice::Text))
    return false;

  file.write(text.toUtf8());
  return true;
}

bool OverseerSession::writeMemory(const QString &text) {
  QFile file(m_memoryPath);

  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate |
                 QIODevice::Text))
    return false;

  file.write(text.toUtf8());
  return true;
}