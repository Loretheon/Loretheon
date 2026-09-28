#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>

enum class Origin {
  User,
  Lore,
};

struct ConductorRequest {
  QString id;
  QString text;
  QDateTime queuedAt;

  Origin origin = Origin::User;

  // "inbox", "routing", "delegated", "awaiting", "done", "failed",
  // "rejected", "skipped"
  //
  // Terminal states: done, rejected, skipped.
  // Non-terminal: inbox, routing, delegated, awaiting.
  // failed is recoverable — the user may retry, remove, or skip.
  QString state = QStringLiteral("inbox");

  QString workerId;
  QString planId;

  QString answer;
  QString rejectReason;

  int retryCount = 0;

  bool deferred = false;

  // Ids of the dependencies that are not yet satisfied, if the request
  // is deferred. Populated by settleDependentRequests.
  QStringList blockedOn;

  QString parentId;
};

struct ConductorWorker {
  QString id;
  QString type;            // "file", "scoped_edit", "memory"
  QString domain;
  QString file;
  QString state;           // "idle", "busy", "waiting", "done"
  int queueDepth = 0;
};