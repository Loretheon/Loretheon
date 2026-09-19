#pragma once

#include <QDateTime>
#include <QString>

// A user request as it appears in the queue and on the conductor's
// board. Immutable except for its state, which the conductor advances.
struct ConductorRequest {
  QString id;              // uuid
  QString text;            // verbatim user text
  QDateTime queuedAt;

  // "inbox", "routing", "delegated", "awaiting", "done", "failed",
  // "rejected"
  QString state = QStringLiteral("inbox");

  // Set when state is "delegated", "awaiting", "done", "failed":
  QString workerId;        // the agent handling it
  QString planId;          // scoped edit plan id, if applicable

  // Set when state is "done" with a non-worker answer, or when state
  // is "rejected":
  QString answer;
  QString rejectReason;

  // How many times this request has been automatically retried after
  // a failure. A value of 1 means the first failure has already been
  // retried once; a second failure is terminal.
  int retryCount = 0;
};

// A worker on the conductor's roster. File agents are persistent;
// scoped edit agents are transient and one per file.
struct ConductorWorker {
  QString id;
  QString type;            // "file" or "scoped_edit"
  QString domain;          // file agents: a directory path or topic
  QString file;            // scoped edit agents: the target file
  QString state;           // "idle", "busy", "waiting", "done"
  int queueDepth = 0;
};