#include "../../include/assistant/AssistantActivity.h"

#include <QDebug>

namespace {

constexpr int kMaxRetained = 200;

QString kindToString(AssistantActivity::Kind kind) {
  switch (kind) {
  case AssistantActivity::Kind::FileOpened:
    return QStringLiteral("opened");
  case AssistantActivity::Kind::FileClosed:
    return QStringLiteral("closed");
  case AssistantActivity::Kind::FileSaved:
    return QStringLiteral("saved");
  case AssistantActivity::Kind::FileEdited:
    return QStringLiteral("edited");
  case AssistantActivity::Kind::ModeSwitched:
    return QStringLiteral("mode");
  case AssistantActivity::Kind::SearchRun:
    return QStringLiteral("searched");
  case AssistantActivity::Kind::ImportRun:
    return QStringLiteral("imported");
  case AssistantActivity::Kind::SpeakInvoked:
    return QStringLiteral("spoke");
  case AssistantActivity::Kind::UserMessage:
    return QStringLiteral("said");
  case AssistantActivity::Kind::SessionStarted:
    return QStringLiteral("started");
  }
  return QStringLiteral("did");
}

} // namespace

AssistantActivity::AssistantActivity(QObject *parent) : QObject(parent) {
  m_batchTimer = new QTimer(this);
  m_batchTimer->setInterval(m_batchIntervalMs);
  m_batchTimer->setSingleShot(false);

  connect(m_batchTimer, &QTimer::timeout, this,
          &AssistantActivity::onBatchTick);
}

AssistantActivity::~AssistantActivity() = default;

QString AssistantActivity::formatEvent(Kind kind, const QString &payload,
                                       const QDateTime &when) {
  QString line = kindToString(kind);

  if (!payload.isEmpty()) {
    line += QLatin1Char(' ') + payload;
  }

  line += QStringLiteral(" @ ")
          + when.toString(QStringLiteral("hh:mm:ss"));

  return line;
}

void AssistantActivity::record(Kind kind, const QString &payload) {
  const QDateTime now = QDateTime::currentDateTimeUtc();
  const QString line = formatEvent(kind, payload.left(120), now);

  m_all.append(line);

  while (m_all.size() > kMaxRetained) {
    m_all.removeFirst();
  }

  m_pending.append(line);

  qDebug() << "[Activity]" << line;
}

QStringList AssistantActivity::recent(int count) const {
  QStringList result;

  const int keep = qMin(count, m_all.size());

  for (int i = m_all.size() - keep; i < m_all.size(); ++i) {
    if (i >= 0) {
      result.append(m_all.at(i));
    }
  }

  return result;
}

void AssistantActivity::startBatchTimer() {
  if (!m_batchTimer->isActive()) {
    m_batchTimer->start();
  }
}

void AssistantActivity::stopBatchTimer() { m_batchTimer->stop(); }

void AssistantActivity::setBatchIntervalMs(int ms) {
  m_batchIntervalMs = qBound(5000, ms, 600000);
  m_batchTimer->setInterval(m_batchIntervalMs);
}

int AssistantActivity::batchIntervalMs() const { return m_batchIntervalMs; }

void AssistantActivity::onBatchTick() {
  if (m_pending.isEmpty()) {
    return;
  }

  const QStringList batch = m_pending;
  m_pending.clear();

  emit batchReady(batch);
}