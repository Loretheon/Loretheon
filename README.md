# Lore

A Qt6/C++ markdown editor with an integrated LLM editing pipeline and a
persistent multi-session assistant called **Overseer**.

## What it is

- **Structured editing.** Files are parsed with tree-sitter. The LLM
  plans edits against named scopes, not raw text ranges.
- **Review-first.** Nothing is written silently. Every change goes
  through plan → generate → review → apply.
- **Overseer.** A persistent assistant with a per-session workspace,
  memory, and sandboxed file tools. It never touches your real notes
  unless you promote a change.

Files open as floating or tiled windows on a canvas. The LLM operates
on them as objects. Closer to a node editor than a tabbed editor.

## Status

### Editor core
- [x] Multi-document tabbed editor
- [x] Per-document cursor, scroll, and undo state
- [x] Tree-sitter parsing (Markdown, Dot, PlantUML, Mermaid)
- [x] Scope-aware highlighting
- [x] Per-document preview (Markdown, HTML, diagrams)
- [x] Two-tier theming

### LLM editing pipeline
- [x] Structured edit planning against named scopes
- [x] Plan → generate → review → apply
- [x] Per-edit accept / reject
- [x] Request-scoped token dispatch
- [x] Non-modal LLM settings (local and remote)
- [x] Grammar-constrained generation for local inference

### Overseer assistant
- [x] Per-session storage
- [x] Sandboxed file tools
- [x] Read-only notes access
- [x] Memory proposals
- [x] Overview references
- [x] `edit_note`
- [x] `edit_workstation_file`
- [x] Focus tracking
- [x] Timeline transcript with jump-to-event ribbon
- [x] Per-window status feedback
- [x] **Conductor** — single routing agent that turns user requests
  into kanban cards and delegates each to a worker
- [x] **Kanban board** — `ConductorBoard`, seven columns
  (Inbox / Routing / Delegated / Awaiting / Done / Failed / Rejected)
  inside the sliding `ConductorDock`
- [x] **Request queue** — `ConductorQueue`, persisted to
  `queue.json`, seven-state lifecycle
- [x] **Worker roster** — `ConductorRoster`, persisted to
  `roster.json`, shown live in the dock
- [x] **Dependency graph** — `DependencyGraph`, persisted as
  `dependencies.dot`, edges declared by the conductor via
  `depends_on`, deferral enforced at dispatch
- [x] **File agents** — persistent workers owning a domain,
  handling read / write / create / list, running a multi-turn
  tool loop until `done` or `fail`
- [x] **Scoped edit agents** — transient, one per file, wrap
  `EditSession`, lock the file while running, called by file
  agents via `delegate_scoped_edit`
- [x] **Write claims** — while an agent is writing a file, any
  request naming that file is redirected to that agent
- [x] **Expertise routing** — a request is routed to the agent whose
  `filesSeen` best matches the files it names, ties broken by
  least-loaded
- [x] **Multi-turn tool loop** — file agents re-prompted with tool
  results until `done` / `fail`
- [x] **Parse-error recovery** — one correction turn on malformed
  JSON before the task is treated as failed
- [x] **Single automatic retry** — a failed task is re-enqueued
  once; a second failure is terminal
- [x] **Terminal failure propagation** — when a task fails after
  retry, its unfinished dependents are marked failed
- [x] **User-removable requests** — a failed card exposes a Remove
  action that prompts about dependents and updates the graph
- [x] **Capacity-aware spawning** — file-agent cap is surfaced to
  the conductor and enforced at dispatch
- [x] **Per-session logs** — conductor, agent, and edit logs written
  under `<session>/logs/`
- [x] **Session splitter state** — the dock's graph/kanban splitter
  geometry is persisted per session
- [x] **Live dependency graph rendering** — the conductor board
  renders the current graph in an SVG panel
- [x] **Concurrent Overseer** — multiple scoped edits in parallel,
  one per file
- [x] **File locking** — a file under edit rejects a second session
- [x] **Review dock** — kanban board for pending work; transcript
  goes read-only while the board is open
- [x] **Notifications** — unified toasts and native notifications

### Workstation UI
- [x] Floating and tiled window modes
- [x] Deterministic tiling layout
- [x] Drag-to-swap and drag-to-float
- [x] Per-session layout persistence
- [x] File watcher with conflict handling
- [x] Auto-hiding left and right rails

### Planned
- [ ] **Text to speech** — read assistant responses and documents aloud
- [ ] **Speech to text** — dictate prompts and notes
- [ ] **Overseer journals** — running narrative of what the assistant is working on
- [ ] **Raster images** — view and edit
- [ ] **Vector images** — view and edit
- [ ] **Audio** — playback and transcription
- [ ] **Git** — native versioning and per-file history
- [ ] **Graph view as a document kind** — a `.mmd` file can be viewed
  as source, as a rendered diagram, or as a structured widget
  (kanban for now, extensible to gantt, timeline, mind-map)
- [ ] **Conductor log rotation** — `conductor.log` grows unbounded
  within a session; cap it or rotate per N entries
- [ ] **Theme PR template** — a `.github/PULL_REQUEST_TEMPLATE/theme.md`
  matching the checklist in `creating-themes.md`

## Supported file types

- [x] Markdown (`.md`)
- [x] Plain text (`.txt`)
- [x] Graphviz (`.dot`, `.gv`)
- [x] PlantUML (`.puml`, `.plantuml`)
- [x] Mermaid (`.mmd`, `.mermaid`)
- [x] HTML (`.html`, `.htm`)