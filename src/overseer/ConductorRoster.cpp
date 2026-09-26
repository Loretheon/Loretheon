#include "../../include/overseer/ConductorRoster.h"

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

ConductorRoster::ConductorRoster(QObject *parent) : QObject(parent) {}

void ConductorRoster::setRosterPath(const QString &path) {
  m_path = path;
  load();
}

void ConductorRoster::add(const ConductorWorker &worker) {
  for (ConductorWorker &existing : m_workers) {
    if (existing.id == worker.id) {
      existing = worker;
      save();
      emit changed();
      return;
    }
  }

  m_workers.append(worker);
  save();
  emit changed();
}

void ConductorRoster::remove(const QString &id) {
  for (int i = 0; i < m_workers.size(); ++i) {
    if (m_workers.at(i).id != id)
      continue;

    m_workers.removeAt(i);
    save();
    emit changed();
    return;
  }
}

void ConductorRoster::setState(const QString &id, const QString &state) {
  for (ConductorWorker &w : m_workers) {
    if (w.id != id)
      continue;

    w.state = state;
    save();
    emit changed();
    return;
  }
}

void ConductorRoster::setQueueDepth(const QString &id, int depth) {
  for (ConductorWorker &w : m_workers) {
    if (w.id != id)
      continue;

    w.queueDepth = depth;
    save();
    emit changed();
    return;
  }
}

ConductorWorker ConductorRoster::byId(const QString &id) const {
  for (const ConductorWorker &w : m_workers) {
    if (w.id == id)
      return w;
  }

  return {};
}

QString ConductorRoster::asPromptSection() const {
  if (m_workers.isEmpty())
    return QStringLiteral("(no workers)");

  QString out;

  for (const ConductorWorker &w : m_workers) {
    if (w.type == QStringLiteral("file")) {
      out += QStringLiteral("%1  domain: %2  state: %3  queue: %4\n")
                 .arg(w.id, w.domain, w.state)
                 .arg(w.queueDepth);
    } else if (w.type == QStringLiteral("scoped_edit")) {
      out += QStringLiteral("%1  file: %2  state: %3\n")
                 .arg(w.id, w.file, w.state);
    }
  }

  return out;
}

void ConductorRoster::load() {
  m_workers.clear();

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

    ConductorWorker w;
    w.id = obj.value(QStringLiteral("id")).toString();
    w.type = obj.value(QStringLiteral("type")).toString();
    w.domain = obj.value(QStringLiteral("domain")).toString();
    w.file = obj.value(QStringLiteral("file")).toString();
    w.state = obj.value(QStringLiteral("state")).toString();
    w.queueDepth = obj.value(QStringLiteral("queueDepth")).toInt();

    if (w.id.isEmpty())
      continue;

    m_workers.append(w);
  }
}

void ConductorRoster::save() {
  if (m_path.isEmpty())
    return;

  QJsonArray arr;

  for (const ConductorWorker &w : std::as_const(m_workers)) {
    QJsonObject obj;
    obj.insert(QStringLiteral("id"), w.id);
    obj.insert(QStringLiteral("type"), w.type);
    obj.insert(QStringLiteral("domain"), w.domain);
    obj.insert(QStringLiteral("file"), w.file);
    obj.insert(QStringLiteral("state"), w.state);
    obj.insert(QStringLiteral("queueDepth"), w.queueDepth);
    arr.append(obj);
  }

  QFile file(m_path);

  if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate |
                 QIODevice::Text))
    return;

  file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
}