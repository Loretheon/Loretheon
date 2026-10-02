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

  // Human-readable description shown on the board. For a user request
  // this is the text the user typed. For a child spawned by a fan-out
  // it is a short label derived from the action, not the action's raw
  // JSON.
  QString text;

  // Raw pre-decided action JSON, empty for user requests. When set,
  // the conductor routes the request by parsing this instead of
  // re-asking the model. text and actionJson carry the same task in
  // two forms: one for humans, one for the router.
  QString actionJson;

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