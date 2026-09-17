# Episteme

A Qt6/C++ markdown editor with an integrated LLM editing pipeline and a
persistent multi-session assistant called **Overseer**.

## What it is

Episteme is a desktop document editor built around the idea that an LLM
should be a first-class collaborator on files, not a chat window bolted on
the side. It combines:

- **Structured editing.** Files are parsed with tree-sitter. The LLM plans
  edits against named scopes (sections, nodes, lines) rather than raw text
  ranges, so edits are validated against the document's actual structure
  before anything is applied.
- **A review-first pipeline.** Nothing is written silently. Every proposed
  change goes through a plan → generate → review → apply flow. The user
  sees the exact scope, the generated content, and can accept or reject
  each edit before it touches disk.
- **Overseer.** A persistent assistant that lives alongside the editor.
  It has a per-session workspace, a memory file, an overview of files the
  user has referenced, a live transcript, and tools that let it read, write,
  and open files inside a sandboxed session folder — never the user's real
  notes, unless the user explicitly promotes a change.

The design is deliberately closer to a **node editor** or a **tiling
window manager** than to a traditional tabbed editor. Files open as
floating or tiled windows on a canvas, the LLM operates on them as
objects, and the user directs it through the same transcript they'd use
to chat.

## Status

### Editor core
- [x] Multi-document tabbed editor
- [x] Per-document cursor, scroll, and undo state
- [x] Tree-sitter structural parsing (Markdown, Dot, PlantUML, Mermaid)
- [x] Scope-aware highlighting
- [x] Per-document preview (Markdown, HTML, diagrams)
- [x] Two-tier theming (independent normal and Overseer themes)

### LLM editing pipeline
- [x] Structured edit planning against named scopes
- [x] Plan → generate → review → apply flow
- [x] Per-edit accept / reject
- [x] Request-scoped token dispatch (no cross-talk between consumers)
- [x] Non-modal LLM settings panel (local and remote)
- [x] Grammar-constrained generation for local inference

### Overseer assistant
- [x] Persistent per-session storage (`memory.md`, `transcript.md`, `overview.md`, `output/`)
- [x] Sandboxed file tools scoped to the session output folder
- [x] Read-only access to the notes root
- [x] Memory proposals with approve / reject
- [x] Overview reference management with dead-reference detection
- [x] `edit_note` — structured edits on session copies of referenced notes
- [x] `edit_workstation_file` — structured edits on files open in the Workstation
- [x] Focus tracking (the LLM edits the file the user is looking at)
- [x] Timeline transcript with a jump-to-event ribbon
- [x] Per-window status feedback (opening, rewriting, review, applied, conflict, …)

### Workstation UI
- [x] Floating and tiled window modes
- [x] Deterministic tiling layout
- [x] Drag-to-swap and drag-to-float
- [x] Per-session layout persistence
- [x] File watcher with conflict detection and reload / keep / overwrite
- [x] Auto-hiding left and right rails

### Planned
- [ ] **Text to speech** — read assistant responses and documents aloud
- [ ] **Speech to text** — dictate prompts and notes
- [ ] **Overseer journals** — the assistant maintains a running narrative of what it's working on, distinct from the raw transcript
- [ ] **Overseer project management** — native project/task structures the LLM reads and writes; changing them triggers LLM events rather than being inert data

## Supported file types

- [x] Markdown (`.md`)
- [x] Plain text (`.txt`)
- [x] Graphviz (`.dot`, `.gv`)
- [x] PlantUML (`.puml`, `.plantuml`)
- [x] Mermaid (`.mmd`, `.mermaid`)
- [x] HTML (`.html`, `.htm`)

### Planned
- [ ] Raster images — view and edit
- [ ] Vector images — view and edit
- [ ] Audio — listening (playback) and transcription via speech-to-text models
- [ ] Git — native versioning and precise per-file history