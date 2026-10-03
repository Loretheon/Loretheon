# Loretheon

**Knowledge. Forever yours.**

Loretheon is a local-first knowledge system with a persistent assistant
named **Lore**. 

[loretheon.app](https://loretheon.app)

- **Lightning speed and efficiency**
> Save on tokens, the AI performs intelligent edits to avoid resending full files.
> Requests are run concurrently so you don't have to wait for the last response.
> We track your context so you never resend information.

- **Conversations that matter**
> Mic up and talk with Lore hands-free. 
> Use the entire application with voice only - your audio never leaves your Computer.
> Industry standard speech recognition and text to speech for free and local.

- **No lock in**
> Bring your own API keys.
> Leave with all your files.

- **Evolving forever**
> Lore knows what's in your notes directory and grows as you do.
> Import PDFs, documents, slides and convert to Markdown.
> Remembers important details and preferences without wasting tokens.

- **Private and free forever**
> Your data is yours.
> Conversations with the LLM are between you and your chosen provider.
> We will never charge you or paywall features.


---

## Contents

- [For everyone](#for-everyone)
  - [What it is](#what-it-is)
  - [What it isn't](#what-it-isnt)
  - [Download](#download)
- [For developers](#for-developers)
  - [Architecture in one page](#architecture-in-one-page)
  - [Building from source](#building-from-source)
  - [Local by default](#local-by-default)
  - [Known limitations](#known-limitations)
- [Feature list](#feature-list)
- [The name](#the-name)
- [License](#license)

---

# For everyone

## What it is

Loretheon has four surfaces, and the assistant wraps all of them.

**Normal mode.** A file explorer, an editor with per-format preview
(Markdown, HTML, diagrams), and a chat panel that proposes structured
edits to whatever you have open. Every proposed change goes through a
review strip before it lands. You drive.

**Overseer mode.** Asynchronous story mapping. You describe a body of
work in one or more requests. The app fans them out, works out the
dependencies on its own, and produces the files. You supervise through
a board and a transcript — or you queue a hundred requests and walk
away.

**Search mode.** Semantic search over your notes. Ask a question, get
an answer synthesized from your own material, with citations.

**The assistant.** Lore lives in a shell you summon with `Ctrl+Space`
and dismiss with `Esc`. She can talk with you, listen to you, and act
on your behalf through every surface above. Large pastes are offloaded
to disk so the conversation stays fast. Long work becomes a job with
an id, so she can start something and keep talking.

The tagline is the pitch: **Knowledge. Forever yours.** Nothing is sent
anywhere you didn't point it at. Nothing is stored in a format you
can't read. Nothing is lost if you close the app and never open it
again.

## What it isn't

- **Not a cloud service.** No account, no telemetry, no proxying.
- **Not a chat window bolted onto an editor.** The assistant is a
  first-class surface with memory, tools, and voice.
- **Not a "second brain."** The files are the point.
- **Not a lock-in.** Leave at any time with everything.

## Download

| Platform | Status | Format |
|---|---|---|
| Linux | Shipping | AppImage |
| macOS | In progress | DMG |
| Windows | In progress | Installer |

Pre-alpha. Behavior, file layouts, and internal APIs will change. See
[Known limitations](#known-limitations).

---

# For developers

## Architecture in one page

Loretheon is a Qt 6 / C++23 desktop application. Four surfaces share
one core:

```
                ┌──────────────────────────────────┐
                │            MainWindow            │
                │  shell stack · modes · toasts    │
                └────────────────┬─────────────────┘
                                 │
        ┌────────────────────────┼────────────────────────┐
        │                        │                        │
┌────▼────┐              ┌────▼────┐              ┌────▼────┐
│ Normal  │              │Overseer │              │ Search  │
│  mode   │              │  mode   │              │  mode   │
└────┬────┘              └────┬────┘              └────┬────┘
│                        │                        │
└────────────────────────┼────────────────────────┘
│
┌────────────────▼─────────────────┐
│     Assistant shell (Lore)       │
│  profile · memory · jobs · tools │
└────────────────┬─────────────────┘
│
┌────────────────▼─────────────────┐
│          InferenceService        │
│  LLM · STT · TTS · embeddings    │
└──────────────────────────────────┘
```

- **Inference layer.** `InferenceService` sits above `LlmClient`,
  `LlamaManager`, `TtsManager` (HeadTTS/Kokoro), `NemoTranscriber`, and
  `EmbeddingModel`. The LLM is local by default (llama.cpp in Docker)
  or any OpenAI-compatible endpoint. Every request carries a stable
  session id so providers with prompt caching can reuse context.
- **Editor core.** `DocumentManager` owns open documents.
  `TextDocument` knows its format. `DocumentArea` hosts the widgets.
  Structure is parsed with tree-sitter into `DocumentNode` trees, so
  edits target named scopes, not text ranges.
- **Edit pipeline.** `EditPlanner` produces a plan, `EditSession`
  resolves it against the document, `EditApplier` applies approved
  edits. The planner's JSON output is constrained by a JSON schema
  that becomes a GBNF grammar on the local path.
- **Overseer.** A conductor classifies each request and dispatches to
  file agents (persistent, own a domain, multi-turn tool loop), scoped
  edit agents (transient, wrap `EditSession`), or a memory agent. A
  dependency graph and a request queue persist to disk per session.
- **Search.** `ScopeIndex` embeds document scopes into a FAISS index.
  `RetrievalLoop` progressively fetches results until an LLM judges
  them sufficient, then answers.

## Building from source

Requirements:

- Qt 6.4 or newer (Core, Gui, Widgets, PrintSupport, Pdf, PdfWidgets,
  Network, Multimedia, MultimediaWidgets, Svg, SvgWidgets, Xml,
  Concurrent, Core5Compat).
- CMake 3.23+ and a C++23 compiler.
- Host tools on `PATH`: `tree-sitter`, `mmdc` (mermaid-cli), `dot`
  (Graphviz).
- OpenMP, ZLIB, BZip2. BLAS and LAPACK on Linux and macOS.
- Node.js, if you want mermaid rendering.

Optional:

- `-DLORE_WITH_AVATAR=ON` builds the 3D avatar. Requires Qt Quick,
  QuickWidgets, Quick3D, and ozz-animation.

Quick start:

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
```

External dependencies (edlib, tree-sitter, tree-sitter-markdown,
lunasvg, quazip, faiss) are built by CMake into `cmake-build-libs/`.
Windows uses vcpkg for faiss; other platforms build it from source.

## Local by default

| Layer | Local path | Remote path |
|---|---|---|
| LLM | llama.cpp via Docker (`LlamaManager`) | Any OpenAI-compatible endpoint |
| Speech-to-text | NeMo.cpp | — |
| Text-to-speech | Kokoro.cpp via HeadTTS | — |
| Embeddings | Local embedding model | — |
| Search | FAISS, on disk | — |

The remote column is opt-in and uses your own credentials. Environment
variables for the remote LLM path (`TALOS_LLM_MODE`, `TALOS_LLM_URL`,
`TALOS_LLM_MODEL`, `TALOS_LLM_API_KEY`, `TALOS_LLM_AUTH`) are read once
on first run and then persisted in settings. They will be renamed in a
future release.

## Known limitations

- Pre-alpha. File layouts, settings keys, and internal APIs will change
  between releases.
- The avatar is opt-in at build time and requires a compositor on
  Linux.
- macOS and Windows builds are in progress.
- Large Markdown files block on layout; section-scoped loading is
  planned.
- Folder imports queue in memory; a mid-import close loses the
  remaining files.
- `conductor.log` grows unbounded within a session.
- Windows builds require vcpkg for faiss.
- The remote LLM environment variables still use the legacy `TALOS_*`
  names.

---

# Feature list

### Editor

- Multi-document editor with per-document cursor, scroll, undo
- tree-sitter parsing: Markdown, DOT, PlantUML, Mermaid
- Scope-aware highlighting
- Per-document preview: Markdown, HTML, diagrams
- Two-tier theming

### LLM editing pipeline

- Structured edit planning against named scopes
- Plan → generate → review → apply
- Per-edit accept and reject
- Grammar-constrained generation for local inference
- Non-modal LLM settings for local and remote providers

### Assistant (Lore)

- Profile files (`identity.md`, `user.md`, `self.md`) always in context
- Memory tree with semantic recall
- Background jobs with ids, per-job abort, global abort
- Tool registry: search, delegate, edit_profile, read_paste, speak,
  promote_note, read_job, list_notes, read_note, write_note, edit_note,
  delete_note
- Restriction invariant: the assistant can tighten its own permissions,
  never loosen them
- Chat outliner with jobs as collapsible children
- Mind map of profile files and topics

### Overseer

- Single conductor that classifies and routes each request
- Create-vs-modify routing
- Eight-column kanban board
- Persisted queue, roster, dependency graph
- `depends_on` on top-level actions, `order` inside batches
- `done` / `rejected` / `skipped` satisfy dependents; `failed` pauses
  them without cascading
- Retry, Skip, Remove on failed cards
- File agents, scoped edit agents, memory agent
- Per-session automation toggles (automatic, autoMemory, autoEdits)

### Search

- Incremental index updates per file
- Debounced reindex on save
- Auto-index on import, promote, delete
- Lazy build at startup
- RetrievalLoop with sufficiency judgment and cited answers

### Speech and voice

- Offline STT via NeMo.cpp, push-to-talk and streaming
- Offline TTS via Kokoro.cpp through HeadTTS
- Voice command registry: dictate, live dictate, read aloud
- Conversation mode with configurable silence threshold
- Viseme-synced lip animation on the avatar

### Sources and notes

- PDF, PPTX, DOCX, HTML, EPUB extractors
- Markdown passthrough
- Provenance frontmatter on extracted notes
- Promote a file or folder from a session into `/notes/<session>/`

### Media

- Raster images with zoom, pan, fit
- SVG through the diagram stack
- Audio playback with transport
- Video playback

### Workstation

- Floating and tiled window modes
- Deterministic tiling, drag-to-swap, drag-to-float
- Per-session layout persistence
- File watcher with conflict handling

### Themes

- Token-based theming shared across all surfaces
- Okabe-Ito palette by default, accessible to all color-vision types
- SvgThemer recolors diagrams to match the active theme

### Planned

- Three-tier agents (file-system agent spawning per-file agents)
- Planner retry on truncated stream
- Overseer journals
- Audio-to-note transcription
- Native git integration with per-file history
- PDF OCR
- Raster and vector image editing
- Bulk queue persistence for folder imports
- Theme PR template

---

# The name

- **Loretheon** is the application.
- **Lore** is the assistant — the character with a profile, a memory
  tree, and a voice.
- **Overseer** is the story-mapping subsystem — the conductor, the
  queue, the workers, the dependency graph.

The old directory name `Episteme` and the old environment prefix
`TALOS_*` are dead and will be renamed. They appear in the source today
only as unfinished cleanup.

---

# License

Pre-alpha. The license is under review and will be settled before the
first public release. The current file in this repository is a
placeholder and does not reflect the intended terms.

---

**Website: [loretheon.app](https://loretheon.app)**
