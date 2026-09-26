#pragma once

#include "ConductorTypes.h"
#include "SessionSettings.h"

#include <QHash>
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

class InferenceService;
class OverseerRunner;
class Workstation;

// Owns every live OverseerRunner. One runner per open session. Lives
// for the lifetime of the application; the view and the assistant
// both talk to it.
//
// The manager is the only place that knows which sessions exist on
// disk and which are currently open. Each session carries a short
// intentional description in its settings.json. The description is
// written when the session is created, by the human or by the model,
// and is read by the model when it chooses where a task belongs.
class OverseerSessionManager : public QObject {
  Q_OBJECT

public:
  struct SessionInfo {
    QString name;
    QString description;
  };

  explicit OverseerSessionManager(InferenceService *inferenceService,
                                  QObject *parent = nullptr);
  ~OverseerSessionManager() override;

  void setWorkstation(Workstation *workstation);

  QStringList listSessions() const;

  // Every session on disk with its description, read from each
  // session's settings.json. Sessions without a description are
  // included with an empty string.
  QList<SessionInfo> listSessionsWithDescriptions() const;

  QString descriptionFor(const QString &name) const;

  bool sessionExists(const QString &name) const;

  // Create a session with an intentional description. Fails when a
  // session with that name already exists, when the name is invalid,
  // or when the description is empty.
  bool createSession(const QString &name, const QString &description);

  OverseerRunner *openSession(const QString &name);

  OverseerRunner *runner(const QString &name) const;

  QString activeSessionName() const { return m_activeSessionName; }
  void setActiveSessionName(const QString &name);

  // Submit a request to a named session on the assistant's behalf.
  // The session must exist. Returns the request id, or an empty
  // string when the session does not exist.
  QString submitToSession(const QString &name, const QString &text,
                          Origin origin);

signals:
  void sessionOpened(const QString &name);
  void sessionClosed(const QString &name);
  void sessionListChanged();

  void requestFinished(const QString &sessionName,
                       const QString &requestId, bool ok,
                       const QString &summary,
                       const QString &filePath);

private slots:
  void onRunnerRequestFinished(const QString &sessionName,
                               const QString &requestId, bool ok,
                               const QString &summary,
                               const QString &filePath);

private:
  InferenceService *m_inferenceService = nullptr;
  Workstation *m_workstation = nullptr;

  QHash<QString, OverseerRunner *> m_runners;

  QString m_activeSessionName;
};