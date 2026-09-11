// PayloadLogger.cpp
#include "../../include/app/PayloadLogger.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QStandardPaths>

PayloadLogger::PayloadLogger(QObject *parent) : QObject(parent) {
  const QString baseDir =
      QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) +
      QStringLiteral("/logs/payloads");

  QDir dir;

  if (!dir.mkpath(baseDir)) {
    qWarning() << "[PayloadLogger] Failed to create log directory:" << baseDir;
  }

  const QString timestamp =
      QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_hhmmss"));

  m_filePath = QStringLiteral("%1/payload_%2.log").arg(baseDir, timestamp);

  m_file.setFileName(m_filePath);

  if (!m_file.open(QIODevice::WriteOnly | QIODevice::Text)) {

    qWarning() << "[PayloadLogger] Failed to open log file:" << m_filePath
               << m_file.errorString();

    return;
  }

  m_stream.setDevice(&m_file);

  qDebug() << "[PayloadLogger] Logging payloads to:" << m_filePath;
}

PayloadLogger::~PayloadLogger() {
  if (m_file.isOpen()) {
    m_stream.flush();
    m_file.close();
  }
}

void PayloadLogger::log(const QString &tag, const QString &content) {
  if (!m_file.isOpen()) {
    return;
  }

  const QString timestamp =
      QDateTime::currentDateTime().toString(Qt::ISODateWithMs);

  m_stream << "[" << tag << "] " << timestamp << "\n" << content << "\n\n";

  m_stream.flush();
}