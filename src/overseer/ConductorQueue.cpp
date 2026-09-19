#include "../../include/overseer/ConductorQueue.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QUuid>

ConductorQueue::ConductorQueue(QObject *parent) : QObject(parent) {}

void ConductorQueue::setQueuePath(const QString &path) {
  m_path = path;
  load();
}

QString ConductorQueue::enqueue(const QString &text) {
  ConductorRequest req;
  req.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  req.text = text;
  req.queuedAt = QDateTime::currentDateTime();
  req.state = QStringLiteral("inbox");

  m_requests.append(req);

  save();

  emit requestAdded(req.id);

  return req.id;
}

bool ConductorQueue::remove(const QString &id) {
  for (int i = 0; i < m_requests.size(); ++i) {
    if (m_requests.at(i).id != id)
      continue;

    m_requests.removeAt(i);
    save();
    emit requestRemoved(id);
    return true;
  }

  return false;
}

void ConductorQueue::setState(const QString &id, const QString &state) {
  for (ConductorRequest &req : m_requests) {
    if (req.id != id)
      continue;

    req.state = state;
    save();
    emit requestChanged(id);
    return;
  }
}

void ConductorQueue::setWorker(const QString &id, const QString &workerId,
                               const QString &planId) {
  for (ConductorRequest &req : m_requests) {
    if (req.id != id)
      continue;

    req.workerId = workerId;
    req.planId = planId;
    save();
    emit requestChanged(id);
    return;
  }
}

void ConductorQueue::setAnswer(const QString &id, const QString &answer) {
  for (ConductorRequest &req : m_requests) {
    if (req.id != id)
      continue;

    req.answer = answer;
    save();
    emit requestChanged(id);
    return;
  }
}

void ConductorQueue::setRejectReason(const QString &id,
                                     const QString &reason) {
  for (ConductorRequest &req : m_requests) {
    if (req.id != id)
      continue;

    req.rejectReason = reason;
    save();
    emit requestChanged(id);
    return;
  }
}

void ConductorQueue::retry(const QString &id) {
  for (ConductorRequest &req : m_requests) {
    if (req.id != id)
      continue;

    req.retryCount += 1;
    req.state = QStringLiteral("inbox");
    req.rejectReason.clear();
    save();
    emit requestChanged(id);
    return;
  }
}

ConductorRequest ConductorQueue::nextInbox() const {
  for (const ConductorRequest &req : m_requests) {
    if (req.state == QStringLiteral("inbox"))
      return req;
  }

  return {};
}

ConductorRequest ConductorQueue::byId(const QString &id) const {
  for (const ConductorRequest &req : m_requests) {
    if (req.id == id)
      return req;
  }

  return {};
}

void ConductorQueue::load() {
  m_requests.clear();

  if (m_path.isEmpty())
    return;

  QFile file(m_path);

  if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
    return;

  const QJsonDocument doc = QJsonDocument::fromJson(file.readAll());

  if (!doc.isArray())
    return;

  for (const QJsonValue &value : doc.array()) {
    if (!value.isObject())
      continue;

    const QJsonObject obj = value.toObject();

    ConductorRequest req;
    req.id = obj.value(QStringLiteral("id")).toString();
    req.text = obj.value(QStringLiteral("text")).toString();
    req.queuedAt = QDateTime::fromString(
        obj.value(QStringLiteral("queuedAt")).toString(), Qt::ISODateWithMs);
    req.state =
        obj.value(QStringLiteral("state")).toString(QStringLiteral("inbox"));
    req.workerId = obj.value(QStringLiteral("workerId")).toString();
    req.planId = obj.value(QStringLiteral("planId")).toString();
    req.answer = obj.value(QStringLiteral("answer")).toString();
    req.rejectReason = obj.value(QStringLiteral("rejectReason")).toString();
    req.retryCount = obj.value(QStringLiteral("retryCount")).toInt(0);

    if (req.id.isEmpty())
      continue;

    m_requests.append(req);
  }
}

void ConductorQueue::save() {
  if (m_path.isEmpty())
    return;

  QJsonArray arr;

  for (const ConductorRequest &req : std::as_const(m_requests)) {
    QJsonObject obj;
    obj.insert(QStringLiteral("id"), req.id);
    obj.insert(QStringLiteral("text"), req.text);
    obj.insert(QStringLiteral("queuedAt"),
               req.queuedAt.toString(Qt::ISODateWithMs));
    obj.insert(QStringLiteral("state"), req.state);
    obj.insert(QStringLiteral("workerId"), req.workerId);
    obj.insert(QStringLiteral("planId"), req.planId);
    obj.insert(QStringLiteral("answer"), req.answer);
    obj.insert(QStringLiteral("rejectReason"), req.rejectReason);
    obj.insert(QStringLiteral("retryCount"), req.retryCount);
    arr.append(obj);
  }

  QFile file(m_path);

  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate |
                 QIODevice::Text))
    return;

  file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
}