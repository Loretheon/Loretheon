#pragma once

#include "ConductorTypes.h"

#include <QObject>
#include <QVector>

// The conductor's request queue, backed by queue.json in the session
// folder. The queue is FIFO. Requests are appended by the UI and
// consumed by the conductor one at a time.
class ConductorQueue : public QObject {
  Q_OBJECT

public:
  explicit ConductorQueue(QObject *parent = nullptr);

  // Set the file path. Loads the queue if the file exists.
  void setQueuePath(const QString &path);

  // Append a new request. Emits requestAdded.
  QString enqueue(const QString &text);

  // Remove a request by id. Returns true if it was present.
  bool remove(const QString &id);

  // Advance the state of a request. Persists on every change.
  void setState(const QString &id, const QString &state);

  void setWorker(const QString &id, const QString &workerId,
                 const QString &planId);

  void setAnswer(const QString &id, const QString &answer);

  void setRejectReason(const QString &id, const QString &reason);

  // Mark a request as deferred because its dependencies are unmet.
  // A deferred request is skipped by the runner's drain loop.
  void setDeferred(const QString &id, bool deferred);

  // Clear the deferred flag on every request. Called by the runner
  // when a request reaches a terminal state, so its dependents can
  // be reconsidered.
  void clearAllDeferred();

  // Increment the retry count for a request and put it back in the
  // inbox so it is picked up again. Used for the single automatic
  // retry after a failure.
  void retry(const QString &id);

  // The first request whose state is "inbox", or an empty request.
  ConductorRequest nextInbox() const;

  // The first request whose state is "inbox" and which is not
  // deferred, or an empty request.
  ConductorRequest nextReadyInbox() const;

  QVector<ConductorRequest> all() const { return m_requests; }

  ConductorRequest byId(const QString &id) const;

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