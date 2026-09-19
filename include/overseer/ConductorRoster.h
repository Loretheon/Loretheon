#pragma once

#include "ConductorTypes.h"

#include <QObject>
#include <QVector>

// The conductor's roster of live workers, backed by roster.json in the
// session folder. File agents are added by the conductor; scoped edit
// agents are added when a scoped edit starts and removed when it ends.
class ConductorRoster : public QObject {
  Q_OBJECT

public:
  explicit ConductorRoster(QObject *parent = nullptr);

  void setRosterPath(const QString &path);

  void add(const ConductorWorker &worker);
  void remove(const QString &id);

  void setState(const QString &id, const QString &state);
  void setQueueDepth(const QString &id, int depth);

  QVector<ConductorWorker> all() const { return m_workers; }
  ConductorWorker byId(const QString &id) const;

  // The roster rendered as the agents-summary section of the
  // conductor's prompt.
  QString asPromptSection() const;

  signals:
    void changed();

private:
  void load();
  void save();

  QString m_path;
  QVector<ConductorWorker> m_workers;
};