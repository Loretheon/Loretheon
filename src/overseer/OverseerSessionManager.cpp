#include "OverseerSessionManager.h"

#include "OverseerRunner.h"
#include "OverseerSession.h"
#include "OverseerStorage.h"
#include "Workstation.h"

#include <QDebug>
#include <QDir>

OverseerSessionManager::OverseerSessionManager(
    InferenceService *inferenceService, QObject *parent)
    : QObject(parent), m_inferenceService(inferenceService) {
  OverseerStorage::ensureRoot();
}

OverseerSessionManager::~OverseerSessionManager() {
  const QStringList names = m_runners.keys();

  for (const QString &name : names) {
    OverseerRunner *r = m_runners.take(name);

    if (r) {
      r->deleteLater();
    }
  }
}

void OverseerSessionManager::setWorkstation(Workstation *workstation) {
  m_workstation = workstation;

  for (OverseerRunner *runner : std::as_const(m_runners)) {
    if (runner)
      runner->setWorkstation(workstation);
  }
}

QStringList OverseerSessionManager::listSessions() const {
  return OverseerSession::list(OverseerStorage::rootPath());
}

QList<OverseerSessionManager::SessionInfo>
OverseerSessionManager::listSessionsWithDescriptions() const {
  QList<SessionInfo> result;

  const QStringList names = listSessions();

  for (const QString &name : names) {
    const QString settingsPath =
        QDir(QDir(OverseerStorage::rootPath())
                 .filePath(QStringLiteral("Sessions")))
            .filePath(QDir(name).filePath(QStringLiteral("settings.json")));

    const SessionSettings settings = SessionSettings::load(settingsPath);

    SessionInfo info;
    info.name = name;
    info.description = settings.description;

    result.append(info);
  }

  return result;
}

QString OverseerSessionManager::descriptionFor(const QString &name) const {
  if (name.isEmpty())
    return {};

  const QString settingsPath =
      QDir(QDir(OverseerStorage::rootPath())
               .filePath(QStringLiteral("Sessions")))
          .filePath(QDir(name).filePath(QStringLiteral("settings.json")));

  return SessionSettings::load(settingsPath).description;
}

bool OverseerSessionManager::sessionExists(const QString &name) const {
  if (name.isEmpty())
    return false;

  return listSessions().contains(name);
}

bool OverseerSessionManager::createSession(const QString &name,
                                           const QString &description) {
  if (name.isEmpty() || description.trimmed().isEmpty())
    return false;

  if (sessionExists(name))
    return false;

  OverseerSession *session =
      OverseerSession::create(OverseerStorage::rootPath(), name, this);

  if (!session) {
    return false;
  }

  SessionSettings settings;
  settings.description = description.trimmed();

  settings.save(session->settingsPath());

  session->deleteLater();

  emit sessionListChanged();
  return true;
}

OverseerRunner *OverseerSessionManager::openSession(const QString &name) {
  if (name.isEmpty())
    return nullptr;

  OverseerRunner *existing = m_runners.value(name, nullptr);

  if (existing)
    return existing;

  if (!sessionExists(name))
    return nullptr;

  auto *runner = new OverseerRunner(m_inferenceService, name, this);

  if (m_workstation)
    runner->setWorkstation(m_workstation);

  connect(runner, &OverseerRunner::requestFinished, this,
          &OverseerSessionManager::onRunnerRequestFinished);

  m_runners.insert(name, runner);

  emit sessionOpened(name);

  return runner;
}

OverseerRunner *OverseerSessionManager::runner(const QString &name) const {
  return m_runners.value(name, nullptr);
}

void OverseerSessionManager::setActiveSessionName(const QString &name) {
  m_activeSessionName = name;
}

QString OverseerSessionManager::submitToSession(const QString &name,
                                                const QString &text,
                                                Origin origin) {
  if (name.isEmpty() || text.trimmed().isEmpty())
    return {};

  if (!sessionExists(name))
    return {};

  OverseerRunner *r = openSession(name);

  if (!r)
    return {};

  if (origin == Origin::Lore)
    return r->submitRequestFromLore(text);

  return r->submitRequest(text);
}

void OverseerSessionManager::onRunnerRequestFinished(
    const QString &sessionName, const QString &requestId, bool ok,
    const QString &summary, const QString &filePath) {
  emit requestFinished(sessionName, requestId, ok, summary, filePath);
}