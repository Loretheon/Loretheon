# Lore

A Qt6/C++ markdown editor with an integrated LLM editing pipeline and a
persistent multi-session assistant called **Overseer**.

- **Structured editing.** Files are parsed with tree-sitter. The LLM
  plans edits against named scopes, not raw text ranges.
- **Review-first.** Nothing is written silently. Every change goes
  through plan → generate → review → apply.
- **Overseer.** A persistent assistant with per-session workspaces,
  memory, and sandboxed file tools. It never touches your real notes
  unless you promote a change.

Files open as floating or tiled windows on a canvas. The LLM operates
on them as objects, not as text buffers.

---

## Table of contents

- [Status](#status)
  - [Editor core](#editor-core)
  - [LLM editing pipeline](#llm-editing-pipeline)
  - [Overseer assistant](#overseer-assistant)
  - [Assistant](#assistant)
  - [Search index](#search-index)
  - [Assistant avatar](#assistant-avatar)
  - [Speech](#speech)
  - [Source ingest](#source-ingest)
  - [Note promotion](#note-promotion)
  - [Media viewing](#media-viewing)
  - [Workstation UI](#workstation-ui)
  - [Planned](#planned)
  - [Potential planned features](#potential-planned-features)
- [Supported file types](#supported-file-types)

---

## Status

### Editor core

- [x] Multi-document editor with per-document cursor, scroll, undo
- [x] Tree-sitter parsing: Markdown, Dot, PlantUML, Mermaid
- [x] Scope-aware highlighting
- [x] Per-document preview: Markdown, HTML, diagrams
- [x] Two-tier theming

### LLM editing pipeline

- [x] Structured edit planning against named scopes
- [x] Plan → generate → review → apply
- [x] Per-edit accept / reject
- [x] Request-scoped token dispatch
- [x] Non-modal LLM settings (local and remote)
- [x] Grammar-constrained generation for local inference

### Overseer assistant

- [x] Per-session storage, sandboxed file tools, read-only notes access
- [x] Memory proposals, overview references
- [x] `edit_note`, `edit_workstation_file`
- [x] Focus tracking, per-window status feedback
- [x] Timeline transcript with jump-to-event ribbon

**Conductor**

- [x] Single routing agent that classifies user requests and
  delegates each to a worker
- [x] Create-vs-modify routing: creating a file uses a file agent,
  any change to an existing file uses a scoped edit
- [x] Automatic single retry on malformed conductor JSON

**Conductor Board and queue**

- [x] Kanban board with eight columns: Inbox / Routing / Delegated
  / Awaiting / Done / Failed / Rejected / Skipped
- [x] `ConductorQueue` persisted to `queue.json`, eight-state
  lifecycle
- [x] `ConductorRoster` persisted to `roster.json`, live in the dock
- [x] Live dependency graph rendered in SVG
- [x] Conductor splitter geometry persisted per session

**Dependencies and failure handling**

- [x] `DependencyGraph` persisted to `dependencies.dot`
- [x] `depends_on` on top-level actions, `order` inside batches
- [x] `done` / `rejected` / `skipped` satisfy dependents
- [x] `awaiting` defers dependents until the user resolves
- [x] `failed` pauses dependents; nothing cascades
- [x] Retry / Skip / Remove actions on failed cards
- [x] `skipped` state distinct from `rejected`
- [x] Removing a failed request deletes its graph node and releases
  its dependents

**Workers**

- [x] **File agents** — persistent, own a domain. Create files and
  directories, read, list. Multi-turn tool loop until `done` /
  `fail`.
- [x] **Scoped edit agents** — transient, one per file, wrap
  `EditSession`. Torn down on apply / cancel / failure.
- [x] **Memory agent** — owns the memory store, one per session.
  Pre-decided proposals skip the LLM.
- [x] Planner watchdog: a stalled stream fails the plan instead of
  hanging

**Scoped edits**

- [x] Applied edits are written back to disk by the runner, not by
  the workstation
- [x] An open workstation window on the edited file is reloaded
- [x] Scoped edit documents are loaded as `DocumentMode::Markdown`,
  so `replace_scope` targets sections, not paragraphs
- [x] Edit-plan cards serialize `replace_scope` and `inside`
  correctly, with real section ids

**Session management**

- [x] `OverseerSessionManager` owns every live `OverseerRunner`
- [x] `OverseerRunner` holds per-session conductor state, view
  independent
- [x] `requestFinished` signal on every terminal transition
- [x] `Origin::User` / `Origin::Lore` markers in the transcript
- [x] Lore delegates to any session by name through the manager
- [x] Session descriptions written at creation, read by the model
- [x] Per-session logs under `<session>/logs/`
- [x] Concurrent Overseer: multiple scoped edits in parallel
- [x] Notifications: unified toasts and native notifications

### Assistant

**Core**

- [x] `LoreAssistant` — profile, memory, index, activity stream,
  tool registry, speech animator, job cache
- [x] Profile: `identity.md`, `user.md`, `self.md`, always in
  context
- [x] Memory tree: `memories/topics/<slug>.md` and
  `memories/sessions/<timestamp>.md`
- [x] Memory index built at startup if missing
- [x] `remember_fact` writes to `user.md`, `self.md`, or the tree
- [x] Turn loop streams the reply and dispatches tool calls

**Jobs**

- [x] Every background job stored in `m_jobs`, keyed by id
- [x] `search` and `delegate` return a job id and do not block
- [x] `read_job` reads by id, blocks if still running
- [x] Per-job abort and global abort

**Notes**

- [x] `list_notes`, `read_note`, `write_note`, `edit_note`,
  `delete_note`
- [x] `promote_note` copies a worker file into `/notes/<session>/`
- [x] Notes are a unit — one topic per file
- [x] Notes are the vault — only `/notes` is indexed

**UI**

- [x] `AssistantWidget` with Chat and Mind tabs
- [x] Chat tab: outliner with user messages as roots, jobs as
  collapsed children
- [x] Streaming into one node
- [x] Job nodes in JetBrains Mono, text nodes in the UI font
- [x] Markdown rendering after the turn finishes
- [x] Status strip: `Idle`, `Thinking`, `N jobs in flight`
- [x] Mind tab: profile files as leaves, topics expand to their
  facts
- [x] Hover to focus a subtree, edge-hover to change depth

**Completion**

- [x] Paste in chat, feed to queue, append to next message, or
  automatic

### Search index

- [x] Incremental add, remove, and refresh per file
- [x] Debounced updates on save
- [x] Auto-index on import, promote, and delete
- [x] Lazy build at startup on first search
- [x] Markdown passthrough copies without modification

### Assistant avatar

- [x] CC Base `Lore.glb` loaded with tinygltf and embedded in the
  Qt resource system
- [x] Custom OpenGL 3.2 renderer inside `QSGRenderNode` in a
  `QQuickWidget`
- [x] ozz-animation skeleton, 10 clips, idle on startup
- [x] Skinning matrices uploaded to a texture buffer object and read
  through a `samplerBuffer`
- [x] Bind pose pushed before any clip plays
- [x] 26 base color textures, per-primitive binding, `GL_REPEAT`
  for atlas bands
- [x] Morph targets blended on the CPU, one `glBufferSubData` per
  viseme change
- [x] `VisemeTable` maps Oculus 15 to Reallusion shapes
- [x] Coarticulation: 40 ms blends between adjacent visemes
- [x] Blink state machine, gaze saccades, expression layer
- [x] Camera orbit rebuilt on parameter change
- [x] Top-level frameless `Qt::Tool` window, stays on top, does not
  take keyboard focus
- [x] Move and resize grips in QML

### Speech

- [x] Offline STT via NeMo-Speech.cpp, push-to-talk
- [x] Streaming STT with interim results
- [x] TTS via HeadTTS, Kokoro ONNX, sentence-sequenced
- [x] Voice command registry with `DictateCommand`,
  `LiveDictateCommand`, `ReadAloudCommand`
- [x] Voice panel toggled by `Ctrl+Shift+Space`
- [x] Viseme sync from HeadTTS timing to morph weights
- [x] TTS reconnect with queued sentences

### Source ingest

- [x] PDF, PPTX, DOCX, HTML, EPUB extractors
- [x] Markdown passthrough
- [x] Provenance frontmatter on extracted notes
- [x] Import Files…, Import Folder…, file-tree context menu, chat
  panel Import
- [x] Threaded extraction, queued imports, collision-safe names
- [x] Sources never modified
- [x] Live extension registry read by tree, dialogs, and folder
  walk

### Note promotion

- [x] Promote a file or folder from a session's output into
  `/notes/<session>/`
- [x] Promote from the file tree, from the assistant via
  `promote_note`
- [x] Promoted files indexed immediately
- [x] Promote reports written, skipped, failed counts

### Media viewing

- [x] Raster images: PNG, JPG, GIF, BMP, WebP, TIFF; zoom, pan,
  fit, 100%, no editing
- [x] SVG through `DiagramView`, matching the diagram stack
- [x] Audio playback with transport and position slider
- [x] Video playback with `QVideoWidget`
- [x] Media dispatch from tree or Open
- [x] Binary file rejection with a toast

### Workstation UI

- [x] Floating and tiled window modes
- [x] Deterministic tiling layout
- [x] Drag-to-swap, drag-to-float
- [x] Per-session layout persistence
- [x] File watcher with conflict handling
- [x] Auto-hiding left and right rails
- [x] File lock as UI signal only; serialization comes from the
  dependency graph

### Planned

- [ ] **Three-tier agents** — a file system agent owns a semantic
  domain and decides what files should exist; it spawns per-file
  agents that own exactly one file and perform its edits. Today's
  file agents own a domain and touch files directly.
- [ ] **Planner retry on truncated stream** — the scoped edit
  planner surfaces a truncated provider response as a failed plan
  instead of retrying once.
- [ ] **Overseer journals** — running narrative of what the assistant
  is working on
- [ ] **Audio to note** — transcription to note via
  `NemoTranscriber`; playback already works
- [ ] **Git** — native versioning and per-file history
- [ ] **Graph view as a document kind** — a `.mmd` file viewed as
  source, rendered diagram, or structured widget
- [ ] **Conductor log rotation** — `conductor.log` grows unbounded
  within a session

### Potential planned features

- [ ] **PDF OCR** — Tesseract fallback for scanned pages;
  `tesseract` and `leptonica` are linked but not called
- [ ] **Bulk queue persistence** — folder imports queue in memory;
  a mid-import close loses the remaining files
- [ ] **Raster image editing**
- [ ] **Vector image editing**
- [ ] **Large-file handling** — opening a very large Markdown file
  blocks on layout. Options: `maximumBlockCount`, a
  `QPlainTextEdit` path, or section-scoped loading
- [ ] **Theme PR template** — a
  `.github/PULL_REQUEST_TEMPLATE/theme.md` matching the checklist
  in `creating-themes.md`

---

## Supported file types

### Native formats (read and write)

| Format | Extensions |
|---|---|
| Markdown | `.md` |
| Plain text | `.txt` |
| Graphviz | `.dot`, `.gv` |
| PlantUML | `.puml`, `.plantuml` |
| Mermaid | `.mmd`, `.mermaid` |
| HTML | `.html`, `.htm` |

### Source formats (read-only ingest)

| Format | Extensions |
|---|---|
| PDF | `.pdf` |
| PowerPoint | `.pptx` |
| Word | `.docx` |
| EPUB | `.epub` |
| Markdown (passthrough) | `.md`, `.markdown` |

### Media formats (view only)

| Kind | Extensions |
|---|---|
| Raster images | `.png`, `.jpg`, `.jpeg`, `.gif`, `.bmp`, `.webp`, `.tiff`, `.tif`, `.ico`, `.ppm`, `.pgm`, `.pbm` |
| Vector images | `.svg` |
| Audio | `.mp3`, `.wav`, `.flac`, `.ogg`, `.m4a`, `.aac`, `.opus` |
| Video | `.mp4`, `.webm`, `.mkv`, `.mov`, `.avi`, `.m4v` |