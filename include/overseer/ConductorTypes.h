#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>

// Where a request came from. User requests are typed into the Overseer
// input. Lore requests are submitted by the assistant on the user's
// behalf. The transcript distinguishes them so the user can see which
// requests they made and which ones Lore made.
enum class Origin {
  User,
  Lore,
};

// A user request as it appears in the queue and on the conductor's
// board.
//
// States:
//
//   inbox      — waiting to be routed. May be deferred if a declared
//                dependency is not yet satisfied.
//   routing    — the conductor is deciding what to do with it.
//   delegated  — handed to a worker (file agent, scoped edit, memory
//                agent). Not yet terminal.
//   awaiting   — the request's terminal effect is a user action. For a
//                memory proposal, the proposal has been recorded and is
//                pending accept/reject. For a scoped edit plan, the
//                plan has been generated and is pending review. Requests
//                in this state are NOT satisfied: a dependent that
//                declared depends_on this request stays deferred until
//                the user resolves it.
//   done       — complete.
//   failed     — something went wrong.
//   rejected   — refused, either by the conductor or by the user.
struct ConductorRequest {
  QString id;              // uuid
  QString text;            // verbatim user text, or pre-decided JSON
  QDateTime queuedAt;

  Origin origin = Origin::User;

  QString state = QStringLiteral("inbox");

  // Set when state is "delegated", "awaiting", "done", "failed":
  QString workerId;        // the agent handling it
  QString planId;          // scoped edit plan id, if applicable

  // Set when state is "done" with a non-worker answer, or when state
  // is "rejected":
  QString answer;
  QString rejectReason;

  // How many times this request has been automatically retried after
  // a failure.
  int retryCount = 0;

  // Set when the conductor has deferred this request because a
  // declared dependency is not yet satisfied. A deferred request is
  // skipped by drainQueue() until the flag is cleared.
  bool deferred = false;

  // When deferred, the ids of the dependencies that are not yet
  // satisfied. Populated by settleDependentRequests so the view can
  // show what the request is waiting on. Empty when not deferred.
  QStringList blockedOn;

  // If non-empty, this request was fanned out from a batch decision
  // made against another request, whose id this holds.
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