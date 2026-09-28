#pragma once

#include "ConductorTypes.h"

#include <QObject>
#include <QVector>

// The conductor's request queue, backed by queue.json in the session
// folder.
class ConductorQueue : public QObject {
  Q_OBJECT

public:
  explicit ConductorQueue(QObject *parent = nullptr);

  void setQueuePath(const QString &path);

  QString enqueue(const QString &text);

  QString enqueueChild(const QString &text, const QString &parentId,
                       Origin origin);

  bool remove(const QString &id);

  QStringList removeChildren(const QString &parentId);

  void setState(const QString &id, const QString &state);

  void setWorker(const QString &id, const QString &workerId,
                 const QString &planId);

  void setAnswer(const QString &id, const QString &answer);

  void setRejectReason(const QString &id, const QString &reason);

  void setDeferred(const QString &id, bool deferred);

  // Record the ids of the dependencies this request is waiting on.
  // Pass an empty list to clear.
  void setBlockedOn(const QString &id, const QStringList &blockedOn);

  void clearAllDeferred();

  // Automatic retry: bumps retryCount. Used by the failure paths that
  // want the "max N attempts" cap to apply.
  void retry(const QString &id);

  // Manual retry: resets retryCount to zero. Used when the user clicks
  // Retry on a failed request. A manual retry is a fresh start.
  void retryFresh(const QString &id);

  // Manual skip: mark the request skipped so that its dependents
  // continue as if the dependency had succeeded. Sets rejectReason to
  // the supplied text so the user can see why.
  void skip(const QString &id, const QString &reason);

  ConductorRequest nextInbox() const;
  ConductorRequest nextReadyInbox() const;

  QVector<ConductorRequest> all() const { return m_requests; }

  ConductorRequest byId(const QString &id) const;

  QVector<ConductorRequest> children(const QString &parentId) const;

  signals:
    void requestAdded(const QString &id);
  void requestChanged(const QString &id);
  void requestRemoved(const QString &id);

private:
  void load();
  void save();

  QString m_path;
  QVector<ConductorRequest> m_requests;
};