# Episteme

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

### Workstation UI
- [x] Floating and tiled window modes
- [x] Deterministic tiling layout
- [x] Drag-to-swap and drag-to-float
- [x] Per-session layout persistence
- [x] File watcher with conflict handling
- [x] Auto-hiding left and right rails

### Planned
- [ ] **Concurrent Overseer** — multiple scoped edits in parallel, one per file
- [ ] **File locking** — a file under edit rejects a second session
- [ ] **Review dock** — kanban board for pending work; transcript goes read-only
- [ ] **Notifications** — unified toasts and native notifications
- [ ] **Text to speech** — read assistant responses and documents aloud
- [ ] **Speech to text** — dictate prompts and notes
- [ ] **Overseer journals** — running narrative of what the assistant is working on
- [ ] **Overseer project management** — native project/task structures the LLM reads and writes
- [ ] **Raster images** — view and edit
- [ ] **Vector images** — view and edit
- [ ] **Audio** — playback and transcription
- [ ] **Git** — native versioning and per-file history

## Supported file types

- [x] Markdown (`.md`)
- [x] Plain text (`.txt`)
- [x] Graphviz (`.dot`, `.gv`)
- [x] PlantUML (`.puml`, `.plantuml`)
- [x] Mermaid (`.mmd`, `.mermaid`)
- [x] HTML (`.html`, `.htm`)