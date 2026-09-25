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
- [x] **Session descriptions** — every session carries an
  intentional one-line description, written at creation by the
  human or the model, read by the model when choosing a session
- [x] **Session manager** — `OverseerSessionManager` owns every live
  `OverseerRunner`; one runner per open session, all running
  concurrently
- [x] **`OverseerRunner`** — per-session conductor state and logic,
  independent of the view. Two sessions run requests at once.
- [x] **`requestFinished`** — every terminal transition emits a
  signal carrying the request id, the outcome, a summary, and any
  output file
- [x] **Origin marker** — requests carry `Origin::User` or
  `Origin::Lore`; the transcript badge shows which
- [x] **Lore delegation** — the assistant submits to any session by
  name through the manager; the transcript shows her requests in the
  same board the user sees

### Assistant
- [x] **`LoreAssistant`** — conductor for the assistant. Owns the
  profile, memory, memory index, activity stream, tool registry,
  speech animator, and job cache.
- [x] **Profile** — `identity.md`, `user.md`, `self.md` under the
  assistant root, read at startup and included verbatim in the
  system prompt on every turn.
- [x] **Memory tree** — `memories/topics/<slug>.md` and
  `memories/sessions/<timestamp>.md`, indexed by `MemoryIndex` and
  recalled semantically.
- [x] **Memory index rebuild** — built at startup if missing, so
  recall works on first run.
- [x] **Always-in-context profile** — the prompt tells her the
  profile is what she knows; she answers from it without searching.
- [x] **`remember_fact`** — writes to `user.md`, `self.md`, or the
  memory tree depending on scope. No approval gate.
- [x] **Turn loop** — `handleUserMessage` builds the messages array,
  sends with tool schemas, streams the reply, dispatches tool calls
  through the registry, and finishes on the last round.
- [x] **Job cache** — every background job stored in `m_jobs` on
  `LoreAssistant`, keyed by id. Results are not pushed to the model.
- [x] **`search` starts a job** — returns a job id, does not block.
- [x] **`read_job`** — reads a job's result by id. Blocks if the job
  is still running. The user can abort the wait.
- [x] **`delegate`** — hands a task to a session through
  `OverseerSessionManager`. Returns a job id.
- [x] **`promote_note`** — copies a worker file into
  `/notes/<session>/` and indexes it.
- [x] **`list_notes`, `read_note`, `write_note`, `edit_note`,
  `delete_note`** — the assistant's direct access to the notes
  folder.
- [x] **Completion policy** — paste in chat, feed to queue, append
  to next user message, or automatic
- [x] **Per-job abort** — the abort control on a job node cancels
  that job and its wait
- [x] **Global abort** — the abort button cancels every in-flight
  job and the current turn
- [x] **`AssistantWidget`** — top-level window with two tabs:
  Chat and Mind
- [x] **Chat tab** — outliner. User messages and assistant replies
  are roots; every job, status, and error is a child of the reply,
  indented and collapsed by default
- [x] **Streaming into one node** — the first delta creates the
  reply node, every later delta appends to it. No duplicate nodes.
- [x] **Tool styling** — job nodes render in JetBrains Mono with a
  kind glyph; text nodes render in the UI font
- [x] **Markdown in replies** — `MarkdownView` renders the body
  once the turn finishes; raw text while streaming
- [x] **Status strip** — always visible in the header: `Idle`,
  `Thinking`, `N jobs in flight`
- [x] **Mind tab** — `MindMapScene` + `MindMapView` render the
  assistant root as a nested graph: profile files as leaves, topics
  expand to their facts
- [x] **No-click navigation** — hovering a node focuses its
  subtree; hovering near the left or right edge of the view zooms
  the visible depth out or in
- [x] **Note promotion** — `NotePromoter` copies files from a
  session output folder into `/notes/<session>/`, skipping
  duplicates. Every promoted file is indexed immediately.
- [x] **Notes are a unit** — small, self-contained, one topic per
  file. Everything else goes to Overseer.
- [x] **Notes are the vault** — only `/notes` is indexed. Overseer
  output is working material; it becomes knowledge only when
  promoted.

### Search index
- [x] **Incremental add** — `ScopeIndex::addFile` indexes one file
  without a rebuild
- [x] **Incremental remove** — `ScopeIndex::removeFile` uses FAISS
  `remove_ids` to drop a file's vectors without re-embedding the
  survivors
- [x] **Refresh** — `refreshFile` removes then re-adds, used on edit
- [x] **Debounced updates** — `markDirty` collects saved files and
  refreshes them on a 3-second timer
- [x] **Auto-index on import** — every successful import calls
  `addFile` on the new note
- [x] **Auto-index on promote** — every promoted file is indexed
- [x] **Auto-index on delete** — `DocumentManager::fileDeleted`
  calls `removeFile`
- [x] **Lazy build at startup** — if the index is missing, the app
  builds it on the first search rather than blocking startup
- [x] **Markdown passthrough** — importing a `.md` file copies it
  into the notes folder unchanged, with no provenance block and no
  added heading

### Assistant avatar
- [x] **CC Base model** — `Lore.glb`, 7 meshes, 18 primitives, 1
  skin, 1969 morph targets, loaded with tinygltf v2.9.0 and
  embedded in the Qt resource system.
- [x] **Custom GL renderer** — OpenGL 3.2 core through
  `QOpenGLFunctions_3_2_Core`, drawn inside a `QSGRenderNode` in a
  `QQuickWidget`. Qt Quick 3D's own model, skeleton, and morph
  elements are not used because 6.4 caps morph targets at 8.
- [x] **Skeleton animation** — ozz-animation, 10 clips (idle, walk,
  walk_left, walk_right, run, run_left, run_right, turn_left,
  turn_right, jump). Idle plays on startup.
- [x] **Skinning** — 101 of 142 skin joints matched to ozz by name;
  the other 41 are `_end` terminals that deform nothing. Skinning
  matrices uploaded to a texture buffer object, indexed through a
  `samplerBuffer` because dynamic indexing of a uniform `mat4`
  array is unreliable on radv.
- [x] **Inverse bind** — used exactly as stored in the GLB. The
  centimetre scale pairs with the ozz rest pose, which is also in
  centimetres; the product is dimensionless and the metre-scale
  vertices are unaffected.
- [x] **Bind pose** — the identity skinning matrix array is pushed
  to the surface before any clip plays, so the model draws correctly
  even when nothing is animating.
- [x] **Textures** — 26 base color textures from the GLB, decoded
  with `QImage` and uploaded as RGBA. Per-primitive binding, with a
  1x1 white fallback for materials that have no base color texture.
  `GL_REPEAT` for atlas bands, image flipped vertically once.
- [x] **Morph targets** — blended on the CPU. The face primitive's
  positions live in their own VBO, rewritten with one
  `glBufferSubData` per viseme change.
- [x] **Viseme table** — maps the Oculus 15 codes to Reallusion
  shapes on the face mesh. `V_Explosive`, `V_Dental_Lip`,
  `V_Affricate`, `V_Open`, `V_Wide`, `V_Tight_O`, `Mouth_Close`,
  `Jaw_Open`, `Mouth_Up`, and the lateral controls.
- [x] **Coarticulation** — `SpeechAnimator` blends between adjacent
  visemes over a 40 ms window at the tail of each, so a held
  consonant does not read as a sustained pose.
- [x] **Blink** — a state machine in `AvatarController`, closing in
  60 ms and opening in 90 ms, scheduled 2 to 6 seconds apart.
- [x] **Expression layer** — brow, cheek, squint, and smile shapes
  driven by the viseme stream and a speaking flag, with a resting
  squint to keep the eyes from reading as too wide.
- [x] **Gaze** — an idle saccade driver that holds a direction for a
  second or two, then jumps to a nearby one. Runs faster during
  speech.
- [x] **Camera** — orbit around a target point, rebuilt on any
  parameter change or geometry change.
- [x] **Widget** — a top-level frameless `Qt::Tool` window, not a
  child of the main window, so the main window's size and aspect
  ratio do not reach it. Stays on top, does not take keyboard
  focus.
- [x] **Move and resize grips** — edge strips and corner grips drawn
  as QML items with hover-only affordances; the drag is driven from
  the widget's own mouse events using global coordinates.

### Speech
- [x] **Speech to text (offline)** — NeMo-Speech.cpp via the C ABI.
  Push-to-talk, transcribed once on release.
- [x] **Speech to text (streaming)** — the NeMo streaming C API
  (`nemo_speech_asr_streaming_recognize` and friends) driven by a
  worker thread. Interim results replace in place; final results
  settle.
- [x] **Text to speech** — HeadTTS server running the timestamped
  Kokoro ONNX model, launched as a child process over a WebSocket.
  Sentence-sequenced through `TtsManager`. Read the current document
  aloud.
- [x] **Voice command registry** — extensible command interface.
  `DictateCommand`, `LiveDictateCommand`, `ReadAloudCommand`
  registered at startup.
- [x] **Voice panel** — non-modal, toggled by `Ctrl+Shift+Space`,
  available in both modes.
- [x] **Viseme sync** — HeadTTS returns Oculus 15 visemes with
  per-phoneme timing, `SpeechAnimator` drives the mouth from the
  playback clock, `AvatarController` maps them to morph weights.
- [x] **TTS reconnect** — if the WebSocket drops, `TtsManager`
  reconnects on a 2 second timer. Sentences handed in while the
  socket is down are queued and flushed on reconnect.

### Source ingest
- [x] **PDF** — text extraction for born-digital PDFs; one section
  per page with `## Page N` headings.
- [x] **PPTX** — slide text from `.pptx` (OOXML in a ZIP); one
  section per slide with `## Slide N` headings.
- [x] **DOCX** — paragraphs and heading styles from `.docx`,
  mapped to Markdown structure.
- [x] **HTML** — readable text from `.html` / `.htm`.
- [x] **EPUB** — spine-ordered chapters from `.epub`, one section
  per chapter with `## Chapter N` headings.
- [x] **Markdown passthrough** — `.md` files are copied into the
  notes folder unchanged.
- [x] **Provenance frontmatter** — each note records source path,
  source hash, extraction timestamp, page count, and MIME type.
  Skipped for Markdown passthrough.
- [x] **Import Files…** — multi-select from the File menu.
- [x] **Import Folder…** — recursive walk, all supported formats,
  source folder structure preserved under the notes folder.
- [x] **File tree context menu** — "Import…" and "Import All…" on
  any supported file.
- [x] **Chat panel Import button** — pick files from the chat
  input row.
- [x] **Threaded extraction** — extraction runs on worker threads;
  results are marshalled back to the main thread.
- [x] **Queued imports** — the folder import keeps the concurrency
  cap and feeds the next file on each completion instead of
  rejecting beyond the cap
- [x] **Collision-safe note names** — importing the same source
  twice produces `lecture.md` and `lecture (2).md`.
- [x] **Sources are never modified** — import reads the source and
  writes a new note; the original file is left untouched.
- [x] **Live extension registry** — the file tree, the file
  dialogs, and the folder walk all read from `IngestRegistry`,
  so a new extractor is offered without a second list.

### Note promotion
- [x] **Promote to notes** — copy a file or folder from a session's
  output into `/notes/<session>/<relative path>`. Skips duplicates,
  never overwrites.
- [x] **Promote from the file tree** — right-click a file or folder
  in a session's output, choose "Promote to notes"
- [x] **Promote from the assistant** — `promote_note` tool, callable
  by the model
- [x] **Promote is indexed** — every promoted file is added to the
  search index immediately
- [x] **Promote reports** — the toast and the tool result say how
  many files were written, skipped, and failed to index

### Media viewing
- [x] **Raster images** — PNG, JPG, GIF (animated), BMP, WebP, TIFF.
  Fit-to-window by default, Ctrl+wheel to zoom anchored at the
  cursor, plain wheel to scroll, right-drag to pan. Fit and 100%
  buttons. No editing.
- [x] **Vector images** — SVG rendered through `DiagramView`, so
  zoom, pan, fit, and keyboard shortcuts match the diagram stack.
  Custom background with a persistent colour and a toggle.
- [x] **Audio** — playback with a transport, position slider, and
  time label.
- [x] **Video** — playback with `QVideoWidget`, position slider,
  and time label.
- [x] **Media dispatch** — opening a media file in the tree or via
  Open routes to the `MediaPane` instead of the text editor.
- [x] **Binary file rejection** — known-binary extensions are
  refused with a toast rather than opened as garbage text.

### Workstation UI
- [x] Floating and tiled window modes
- [x] Deterministic tiling layout
- [x] Drag-to-swap and drag-to-float
- [x] Per-session layout persistence
- [x] File watcher with conflict handling
- [x] Auto-hiding left and right rails

### Planned
- [ ] **Overseer journals** — running narrative of what the assistant
  is working on
- [ ] **Audio** — transcription to note via `NemoTranscriber`; the
  transcript becomes a note. Playback already works.
- [ ] **Git** — native versioning and per-file history
- [ ] **Graph view as a document kind** — a `.mmd` file can be viewed
  as source, as a rendered diagram, or as a structured widget
  (kanban for now, extensible to gantt, timeline, mind-map)
- [ ] **Conductor log rotation** — `conductor.log` grows unbounded
  within a session; cap it or rotate per N entries

### Potential planned features
- [ ] **Source ingest — PDF OCR** — Tesseract fallback for scanned
  PDFs and photographed pages. `tesseract` and `leptonica` are
  already linked; the extractor does not call them yet.
- [ ] **Source ingest — bulk queue persistence** — the folder
  import now queues in memory. If the app is closed mid-import, the
  remaining files are lost.
- [ ] **Raster images — editing**
- [ ] **Vector images — editing**
- [ ] **Large-file handling** — opening a very large Markdown file
  blocks the UI while `QTextDocument` lays out every block. Options
  under consideration: `maximumBlockCount`, a `QPlainTextEdit` path
  for oversized files, or section-scoped loading driven by
  `DocumentStructure`.
- [ ] **Theme PR template** — a `.github/PULL_REQUEST_TEMPLATE/theme.md`
  matching the checklist in `creating-themes.md`

## Supported file types

### Native formats (read and write)
- [x] Markdown (`.md`)
- [x] Plain text (`.txt`)
- [x] Graphviz (`.dot`, `.gv`)
- [x] PlantUML (`.puml`, `.plantuml`)
- [x] Mermaid (`.mmd`, `.mermaid`)
- [x] HTML (`.html`, `.htm`)

### Source formats (read-only ingest)
- [x] PDF (`.pdf`)
- [x] PowerPoint (`.pptx`)
- [x] Word (`.docx`)
- [x] EPUB (`.epub`)
- [x] Markdown (`.md`, `.markdown`) — passthrough

### Media formats (view only)
- [x] Raster images (`.png`, `.jpg`, `.jpeg`, `.gif`, `.bmp`, `.webp`, `.tiff`, `.tif`, `.ico`, `.ppm`, `.pgm`, `.pbm`)
- [x] Vector images (`.svg`)
- [x] Audio (`.mp3`, `.wav`, `.flac`, `.ogg`, `.m4a`, `.aac`, `.opus`)
- [x] Video (`.mp4`, `.webm`, `.mkv`, `.mov`, `.avi`, `.m4v`)