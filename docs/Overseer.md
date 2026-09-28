# Overseer — Technical Overview

## What it is

Overseer is one half of Lore, the other half being Normal mode. Normal
mode is a single-document editor with LLM-assisted structural edits.
Overseer is the multi-request side: you type a sequence of requests into
a session and the application routes each one to the right worker,
executes it, and reports back.

The two modes share the underlying edit pipeline, the memory store, the
inference service, and the assistant. They differ in where the user is
looking. Normal mode edits the document you have open. Overseer edits
files in a session folder, which you may never open.

A session is a folder on disk. Requests, transcript, memory, and outputs
all live under that folder. Overseer runs sessions in parallel; each has
its own queue, its own conductor, its own workers, and its own
transcript.

---

## The three actors

Overseer has exactly three kinds of thing doing work.

### The conductor

The conductor is the router. It receives one user request at a time and
decides what to do with it. It does no work itself. Its decision is a
single JSON object, one of:

- `answer` — reply directly to the user.
- `reject` — refuse with a reason.
- `propose_memory` — add, replace, or delete a memory fact.
- `spawn_scoped_edit` — plan a structural edit to a specific file.
- `spawn_file_agent` — create a new file agent for a domain.
- `route_to_worker` — hand the request to an existing agent.
- `batch` — the request is really several actions at once.

The conductor is an LLM. It is given a prompt built from the session's
current state: memory facts, pending proposals, the file tree, the
agent roster, recent conductor actions, and the request queue.

### The file agents

A file agent owns a domain — a directory of related files. It reads,
writes, creates, and lists files under its domain. It runs its own
multi-turn loop: each turn it emits one JSON object naming a tool call
or a terminal action, and the harness executes it and feeds the result
back.

File agents delegate structural edits. If a task requires an insert,
replace, or delete on the content of an existing file the agent did not
create, it emits `delegate_scoped_edit` and the request is handed to a
scoped edit agent.

### The memory agent

The memory agent owns the memory store. It is constructed once per
session and lives for the lifetime of the session. Every memory
operation goes through it, whether the trigger is the conductor's
`propose_memory` or a user clicking accept on a proposal card.

Memory proposals are pre-decided when they come from the conductor: the
agent applies the action directly without an LLM round-trip. Free-form
memory instructions can also be handed to it, in which case it reasons
about the operation and emits `add_fact`, `replace_fact`, or
`delete_fact`.

### The scoped edit agent

A scoped edit agent plans and applies a structural edit to one file. It
is transient: created when a scoped edit is requested, torn down when the
plan is applied or cancelled.

Planning and applying are separate. The planner reads the file, resolves
the user's intent against the file's structure, and emits a plan of
`EditCommand` objects. The user reviews the plan through the transcript's
edit-plan card. On approval, the plan is applied to the file on disk.

The scoped edit operates on a `TextDocument` loaded from the file. It
does not require the file to be open in any editor. This is the
difference between Overseer and Normal mode: Normal mode scoped edits run
against the document in the visible editor, so the user can see the
pending-edit highlights and accept or reject them in place. Overseer
scoped edits run against a document loaded from disk and are reviewed
through the transcript.

---

## How a request flows

1. **Enqueue.** `OverseerRunner::submitRequest` appends a
   `ConductorRequest` to the queue and writes it to `queue.json`.

2. **Drain.** `drainQueue` picks the first ready request from the queue
   — the first request in `inbox` state that is not deferred — and marks
   it `routing`. It sends the conductor a prompt.

3. **Route.** The conductor replies with one JSON object. The runner
   parses it and decides what to do.

4. **Fan out or apply.** A single action becomes a dispatch to a worker.
   A `batch` fans out into N child requests, one per sub-action, each
   enqueued in the same queue with the parent's id as `parentId`.

5. **Execute.** Workers do their work. File agents run multi-turn LLM
   loops. Memory proposals become pending cards. Scoped edits plan and
   wait for review.

6. **Resolve.** Each request reaches a terminal state — `done`,
   `failed`, or `rejected` — either on its own or after the user accepts
   or rejects a pending action.

7. **Settle dependents.** Whenever a request reaches a terminal state,
   the runner re-evaluates every deferred request. Those whose
   dependencies are now satisfied become eligible and are picked up by
   the next drain.

---

## Dependencies

Requests may depend on other requests. A dependency is an edge in a
graph stored in `dependencies.dot` inside the session folder. It is a
plain DOT file and can be read or edited by hand.

Two ways to declare a dependency:

- **Top-level request.** Any single action object (not inside a batch)
  may carry a `depends_on` array of request ids. The ids must refer to
  requests that already exist.

- **Inside a batch.** The batch object carries a top-level `order`
  array. Each entry is `[<dependent position>, [<prerequisite position>,
  ...]]`, where positions are 1-based indices into the `actions` array.
  After fan-out, the indices are translated into real edges between the
  child requests.

A request whose dependencies are not yet satisfied sits in `inbox` with
`deferred = true`. It is skipped by the drain loop until its
dependencies are `done`. Dependencies are resolved in three states:

- `done` — satisfied.
- `failed` or `rejected` — the dependent fails immediately with
  `"Dependency X did not complete."`
- `awaiting` — **not satisfied**. A request that is waiting on a user
  action is not terminal, so its dependents stay deferred until the user
  resolves it.

If a dependency fails, the runner fires a `NeedsUserInput` notification
and the user chooses whether to retry the failed request or remove it and
let the dependents fail.

---

## Memory

Memory is stored in two files:

- **Global** — `Overseer/memory.md` under the app data directory, shared
  across every session.
- **Session** — `memory.md` inside the session folder.

Both files are Markdown. Facts are lines under a `## Accepted proposals`
heading. The prose block above the section is free-form and untouched by
the memory agent.

### Proposals

Every memory operation is a proposal until the user accepts it. A
proposal has a status:

- `pending` — waiting on the user.
- `accepted` — written to memory.
- `rejected` — discarded.
- `failed` — the memory agent could not resolve or apply it.

Proposals are persisted to `proposals.json` in the session folder.

A request whose result is a pending proposal moves to `awaiting`. It is
not terminal until the user accepts or rejects. Dependents that declared
`depends_on` it stay deferred.

### Fact keys

Each fact has a deterministic key computed as `qHash(fact | scope)`.
The key is stable across edits as long as the fact text and scope do not
change. The key is shown alongside each fact in the conductor's prompt
as `[<key>] <fact>`. The conductor cites keys when proposing a replacement
or deletion.

The memory agent tolerates three shapes of `replaces` value:

- A numeric key. Used directly.
- The verbatim text of a fact. Resolved to its key by scanning the
  current facts.
- A named hint. If exactly one current fact contains the hint as a whole
  word, that fact is used. Otherwise the operation degrades to an add.

If a replace targets a fact that is no longer present, the operation
falls back to an insert and a note is attached to the proposal. If a
delete targets a fact that is no longer present, the operation is a
successful no-op.

---

## The transcript

Every request, every proposal, every plan, every message from a worker,
and every reply is written to the transcript as a `TranscriptEvent`. The
transcript is persisted to `transcript.md` in the session folder.

The transcript's card renderer (`TranscriptEventCard`) presents each
event type differently. Memory proposals get an interactive card with
scope, accept, and reject controls. Edit plans get an edit-plan card with
per-edit accept and reject. Assistant messages are rendered as Markdown.

The side panel's "User actions" tab shows every pending action across the
session — memory proposals and pending edit plans — as a single queue.
The side panel and the transcript are two views of the same underlying
state. Accepting or rejecting from either updates the other.

---

## The Conductor Board

The Conductor Board is a kanban view of the request queue, grouped by
state: Inbox, Routing, Delegated, Awaiting, Done, Failed, Rejected. Each
card shows the request text, its current state, and any available
actions. `blockedOn` requests display what they are waiting on.

Above the kanban is the dependency graph, rendered as SVG from the DOT
file. Nodes are request ids with their text as a label. Edges are
dependencies.

The board is not the place to see *why* the conductor made a decision.
That is in the transcript. The board is the place to see what is
happening right now.

---

## The session layout

A session folder contains:

```
Sessions/<name>/
output/                session output files
transcript.md          the transcript
overview.md            references for the conductor
memory.md              session-scoped memory
settings.json          per-session automation toggles
queue.json             the request queue
roster.json            the worker roster
dependencies.dot       the dependency graph
proposals.json         memory proposals
agents/conductor.json  recent conductor actions
logs/                  per-session logs
conductor_splitter.json  board layout
```

Everything is a plain file. The session can be inspected, copied, or
deleted with normal filesystem tools.

---

## The conductor's vocabulary

The conductor must reply with exactly one JSON object. The shapes:

```json
{"action": "answer", "text": "..."}

{"action": "reject", "reason": "..."}

{"action": "propose_memory",
 "fact": "...",
 "rationale": "...",
 "scope": "global" | "session"}

{"action": "propose_memory",
 "fact": "<new text>",
 "rationale": "...",
 "scope": "global" | "session",
 "replaces": "<key>"}

{"action": "propose_memory",
 "fact": "",
 "scope": "global" | "session",
 "replaces": "<key>"}

{"action": "batch",
 "actions": [ {...}, {...}, ... ],
 "order": [ [<dependent>, [<prerequisite>, ...]], ... ]}

{"action": "spawn_scoped_edit",
 "file": "recipe.md",
 "instruction": "..."}

{"action": "spawn_file_agent",
 "domain": "recipes",
 "instruction": "..."}

{"action": "route_to_worker",
 "agent": "agent-fs-2",
 "instruction": "..."}
```

Any single action may also carry a top-level `depends_on` array of
request ids.

Inside a batch, do not use `route_to_worker`: agent ids do not exist yet
at the moment the batch is written. Use `spawn_file_agent` and let the
dispatcher route to an existing agent by expertise.

---

## The file agent's vocabulary

A file agent replies with exactly one JSON object per turn:

```json
{"action": "call_tool",
 "tool": "write_file",
 "args": {"path": "...", "content": "..."}}

{"action": "call_tool",
 "tool": "read_file",
 "args": {"path": "..."}}

{"action": "call_tool",
 "tool": "list_directory",
 "args": {"path": "..."}}

{"action": "call_tool",
 "tool": "create_directory",
 "args": {"path": "..."}}

{"action": "delegate_scoped_edit",
 "file": "<path>",
 "instruction": "..."}

{"action": "done", "summary": "..."}

{"action": "fail", "reason": "..."}
```

`write_file` overwrites. To change a file the agent created earlier in
the same task, call `write_file` again with the full new contents.
Structural edits to files the agent did not create must be delegated.

---

## The memory agent's vocabulary

The memory agent replies with exactly one JSON object per turn:

```json
{"action": "list_facts", "scope": "global" | "session"}

{"action": "add_fact",
 "fact": "...",
 "rationale": "...",
 "scope": "global" | "session"}

{"action": "replace_fact",
 "replaces": "<key>",
 "fact": "<new text>",
 "rationale": "...",
 "scope": "global" | "session"}

{"action": "delete_fact",
 "replaces": "<key>",
 "scope": "global" | "session"}

{"action": "done", "summary": "..."}

{"action": "fail", "reason": "..."}
```

Pre-decided memory actions from the conductor skip the LLM entirely and
are applied directly.

---

## Automation

Each session has a settings file with three toggles:

- `automatic` — master switch. When on, forces the other two on.
- `autoMemory` — accept every memory proposal without asking.
- `autoEdits` — apply every edit plan without asking.

When auto-memory is on, a memory proposal is written immediately and the
request moves straight to `done`. When off, the request moves to
`awaiting` until the user accepts or rejects.

When auto-edits is on, a generated edit plan is applied without review.
When off, the plan moves to `awaiting` and the user reviews it through
the transcript card.

Settings are reloaded from disk on every drain, so toggling a switch
takes effect on the next request without reopening the session.

---

## Failure handling

Workers are retried up to three times on failure. A failed file agent
task is re-dispatched to the same agent so its state carries over. A
failed memory agent task is retried the same way.

Parse failures in worker output get one correction turn. The offending
raw response is appended to the prompt and the model is asked to reply
with valid JSON only.

A failed dependency fails its dependents. The runner fires a
`NeedsUserInput` notification and gives the user the choice to retry the
failed request or remove it. Removing it fails the dependents with
`"Dependency X was removed."`

---

## What Overseer is not

Overseer is not a chat window. It is a queue with a conductor. Requests
are independent items, not turns in a conversation. The conductor's job
is to classify each request and route it; it is not to be a personality
or to accumulate conversational context.

Overseer is not a planner. It does not build a task graph in advance. The
conductor decides one request at a time and the graph emerges from the
decisions.

Overseer is not a monitor for the user. The board exists so the user can
see the current state at a glance. Everything the user needs to act on
appears in the side panel's "User actions" tab and, if the app is not
focused, as a system notification.

Overseer does not own the files. The session folder is an ordinary
directory. Files can be edited by hand, opened in other tools, copied,
or deleted. The session's state is derived from those files on every
reopen.
