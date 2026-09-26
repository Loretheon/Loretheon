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

QString ConductorQueue::enqueueChild(const QString &text,
                                     const QString &parentId,
                                     Origin origin) {
  ConductorRequest req;
  req.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
  req.text = text;
  req.queuedAt = QDateTime::currentDateTime();
  req.state = QStringLiteral("inbox");
  req.parentId = parentId;
  req.origin = origin;

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

QStringList ConductorQueue::removeChildren(const QString &parentId) {
  if (parentId.isEmpty())
    return {};

  QStringList removed;

  for (int i = m_requests.size() - 1; i >= 0; --i) {
    if (m_requests.at(i).parentId != parentId)
      continue;

    removed.append(m_requests.at(i).id);
    m_requests.removeAt(i);
  }

  if (removed.isEmpty())
    return removed;

  save();

  for (const QString &id : std::as_const(removed))
    emit requestRemoved(id);

  return removed;
}

void ConductorQueue::setState(const QString &id, const QString &state) {
  for (ConductorRequest &req : m_requests) {
    if (req.id != id)
      continue;

    req.state = state;

    if (state != QStringLiteral("inbox")) {
      req.deferred = false;
      req.blockedOn.clear();
    }

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

void ConductorQueue::setDeferred(const QString &id, bool deferred) {
  for (ConductorRequest &req : m_requests) {
    if (req.id != id)
      continue;

    if (req.deferred == deferred)
      return;

    req.deferred = deferred;

    if (!deferred)
      req.blockedOn.clear();

    save();
    emit requestChanged(id);
    return;
  }
}

void ConductorQueue::setBlockedOn(const QString &id,
                                  const QStringList &blockedOn) {
  for (ConductorRequest &req : m_requests) {
    if (req.id != id)
      continue;

    if (req.blockedOn == blockedOn)
      return;

    req.blockedOn = blockedOn;
    save();
    emit requestChanged(id);
    return;
  }
}

void ConductorQueue::clearAllDeferred() {
  bool any = false;

  for (ConductorRequest &req : m_requests) {
    if (!req.deferred)
      continue;

    req.deferred = false;
    req.blockedOn.clear();
    any = true;
  }

  if (!any)
    return;

  save();
}

void ConductorQueue::retry(const QString &id) {
  for (ConductorRequest &req : m_requests) {
    if (req.id != id)
      continue;

    req.retryCount += 1;
    req.state = QStringLiteral("inbox");
    req.deferred = false;
    req.blockedOn.clear();
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

ConductorRequest ConductorQueue::nextReadyInbox() const {
  for (const ConductorRequest &req : m_requests) {
    if (req.state != QStringLiteral("inbox"))
      continue;

    if (req.deferred)
      continue;

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

QVector<ConductorRequest> ConductorQueue::children(
    const QString &parentId) const {
  QVector<ConductorRequest> result;

  if (parentId.isEmpty())
    return result;

  for (const ConductorRequest &req : m_requests) {
    if (req.parentId == parentId)
      result.append(req);
  }

  return result;
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
    req.deferred = obj.value(QStringLiteral("deferred")).toBool(false);
    req.parentId = obj.value(QStringLiteral("parentId")).toString();

    const QJsonArray blockedArray =
        obj.value(QStringLiteral("blockedOn")).toArray();

    for (const QJsonValue &bv : blockedArray) {
      const QString blockedId = bv.toString();
      if (!blockedId.isEmpty())
        req.blockedOn.append(blockedId);
    }

    const QString originString =
        obj.value(QStringLiteral("origin")).toString();
    req.origin = originString == QStringLiteral("lore") ? Origin::Lore
                                                        : Origin::User;

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
    obj.insert(QStringLiteral("deferred"), req.deferred);

    QJsonArray blockedArray;
    for (const QString &b : req.blockedOn)
      blockedArray.append(b);
    obj.insert(QStringLiteral("blockedOn"), blockedArray);

    obj.insert(QStringLiteral("parentId"), req.parentId);
    obj.insert(QStringLiteral("origin"),
               req.origin == Origin::Lore ? QStringLiteral("lore")
                                          : QStringLiteral("user"));
    arr.append(obj);
  }

  QFile file(m_path);

  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate |
                 QIODevice::Text))
    return;

  file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
}