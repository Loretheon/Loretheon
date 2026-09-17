// PayloadLogger.h
#pragma once

#include <QFile>
#include <QObject>
#include <QTextStream>

class PayloadLogger : public QObject {
  Q_OBJECT

public:
  explicit PayloadLogger(QObject *parent = nullptr);

  ~PayloadLogger() override;

  void log(const QString &tag, const QString &content);

  QString filePath() const { return m_filePath; }

private:
  QString m_filePath;
  QFile m_file;
  QTextStream m_stream;
};