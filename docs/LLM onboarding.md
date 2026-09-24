# Lore — LLM Onboarding

Paste this as the first message in a new chat.

---

## What Lore is

Lore is a **Personal Knowledge Management System (PKMS)** enhanced
with AI. A PKMS is a system for building a body of knowledge over
years, where the files are the point and the software stays out of
the way.

Core principles:

- **Semantics over syntax.** Intent matters, not punctuation.
- **FOSS and BYOK.** Local models are first-class.
- **Your files are yours.** Ordinary files in ordinary directories.
  Leave at any time.
- **Two modes, one system.** Normal mode is an editor with
  LLM-assisted structured edits. Overseer mode is a conductor and a
  roster of agents producing notes in parallel.
- **Graphing languages are first-class.** Markdown, plaintext, DOT,
  PlantUML, Mermaid.
- **A resident, not a panel.** The assistant is a character who
  lives in the corner of the window, speaks, listens, and remembers.
  She is not a chat widget.

## How we work

**Full files, always.** Every reply that touches a file returns the
complete file. No diffs, no fragments, no line ranges. A one-line
change still returns the whole file. Length is not a reason to
abbreviate.

**Never guess an API.** If a signature, field, or return type is
unknown, ask for the header via `xclip`. Stop before writing a call
to a function you have not seen. A wrong guess costs a build; asking
costs one command.

**Ask via xclip.** When you need files, give one `find` command that
dumps to the clipboard. Debian 12, `xclip` assumed. Multiple files
go in one `find` with several `-o -name` clauses.

```
find ./include ./src -type f \( \
-name 'Foo.h' -o -name 'Foo.cpp' \
\) -not -path './cmake-build-*' \
-print0 | xargs -0 -I{} sh -c 'echo "===== {} ====="; cat "{}"; echo' \
| tee /tmp/deps.txt | xclip -selection clipboard
```

**Diagnose, don't shotgun.** One hypothesis at a time. State what
would confirm or refute it. Ask for the minimum.

**Short.** A few sentences, then the code.

**One change at a time.** Verify before the next.

**Read the log before the next change.** The application writes its
state to the terminal. A wrong build shows up there before it shows
up on screen. Read it carefully before deciding what to do next.

## Qt and platform notes

The project is on **Qt 6.4.2**, from Debian packages. Qt 6.4 is not
Qt 6.6. Several Qt 3D APIs that exist in 6.6 or later do not exist
in 6.4, and the project does not use them. If a signature is in
doubt, ask for the header from
`/usr/include/x86_64-linux-gnu/qt6/` before writing a call.

The application runs on X11 with KWin. A top-level window with a
transparent background needs a compositor. KWin is one. The avatar
is a top-level frameless window; if it shows a black background, the
cause is compositing, not the code.

## Single sources of truth

Some values live in exactly one place and must not be duplicated:

- **The camera** lives in the QML scene (`AvatarOverlay.qml`). It is
  not duplicated in C++.
- **The avatar widget's size and offset** live in `QSettings`,
  written on move and resize. They are not recomputed from the main
  window.
- **The viseme table** lives in `VisemeTable.cpp`. It is the one
  place a viseme-to-shape mapping is written.

When a value seems to be wrong in more than one place, the fix is to
find the duplication and remove it, not to override at each site.

## The Overseer workflow

A user request enters a queue. The **conductor** answers with one of:

- `answer` — reply directly.
- `reject` — refuse with a reason.
- `spawn_scoped_edit` — structural edit to one file.
- `spawn_file_agent` — new worker for a new domain.
- `route_to_worker` — existing worker handles it.

It may also add a `depends_on` array.

The dispatcher applies four rules in order:

1. Unmet dependency — waits in the inbox.
2. Write claim — the writer of a named file gets it.
3. Expertise — the agent whose `filesSeen` matches, ties by
   least-loaded.
4. Fallback — the conductor's choice, then least-loaded, then a new
   agent.

Two worker types:

- **File agents** are persistent, own a domain, handle read / write /
  create / list, run a multi-turn tool loop, delegate structural
  edits.
- **Scoped edit agents** are transient, one per file, wrap
  `EditSession`, plan and apply on approval.

## The assistant

The assistant is the resident character named Lore. She is owned by
`LoreAssistant`, which is constructed once by `MainWindow` and lives
for the duration of the application. She holds:

- a profile (three small files: identity, user, self)
- a memory tree (session files and topic files)
- a private semantic index over that memory
- an activity stream of the user's actions
- a registry of tools she can call
- a speech animator that drives her mouth

She is wired to the application through `LoreAssistant::Config`,
which carries the inference service, the avatar, the document
manager, the search service, and her root path. She owns none of
those; they are held as raw pointers for her lifetime.

Tools the assistant can call:

    search_notes       search the user's notes by meaning
    open_file          open a note in the editor
    insert_text        insert text at the cursor
    remember_fact      propose a fact for the memory tree
    change_setting     change one of her own settings
    speak              speak a line aloud
    set_expression     change the avatar's facial expression

`insert_text` and `change_setting` are destructive. When they run,
the user is asked to approve. `change_setting` has a second guard in
the settings layer itself: the assistant can only make her own
restrictions tighter, never looser. That is enforced in code, not in
the tool.

## The avatar

The avatar is a Reallusion Character Creator Base figure, exported
to glTF as `Lore.glb`, skinned and animated at runtime, and drawn
with a custom OpenGL renderer. It is not a Qt Quick 3D scene graph
model.

The layers:

- `AvatarWidget` — a `QQuickWidget`. Hosts the QML scene.
- `AvatarOverlay.qml` — the scene. One `AvatarSurface` and the grip
  affordances.
- `AvatarSurface` — a `QQuickItem` with a camera. Owns the view and
  projection matrices.
- `AvatarRenderNode` — a `QSGRenderNode`. The correct hook for
  drawing inside a `QQuickWidget`.
- `AvatarRenderer` — the GL pipeline. Shaders, VAOs, textures.
- `AvatarController` — owns ozz, the skin binding, the morph
  weights, and the facial layers.
- `AvatarMeshLoader` — reads `Lore.glb` with tinygltf.
- `VisemeTable` — maps Oculus viseme names to Reallusion shape key
  indices.

Skinning matrices are uploaded to a texture buffer object and read
in the vertex shader through a `samplerBuffer`. Dynamic indexing of
a uniform `mat4` array is not reliable on all GL 3.2 drivers.

Morph targets are blended on the CPU. The face primitive's position
lives in its own VBO, so a viseme change is one `glBufferSubData`.

Textures are uploaded once, at initialize, and sampled in the
fragment shader. The GLB stores the body, arm, leg, and nails
textures packed into atlas bands with U offsets above 1.0, so those
textures use `GL_REPEAT`.

## Voice and visemes

Speech is synthesised by HeadTTS, a Node.js server that runs the
timestamped Kokoro ONNX model and returns audio, Oculus visemes, and
per-viseme timing. It runs as a child process, started by
`TtsManager` on first use.

The viseme pipeline:

1. `LoreAssistant::say` calls `InferenceService::speak`.
2. `InferenceService` hands the text to `TtsManager`.
3. `TtsManager` sends a `synthesize` message over a WebSocket.
4. HeadTTS replies with viseme codes, start times, and durations,
   then with the audio.
5. `TtsManager` builds an `AudioChunk` with the audio and a viseme
   timeline, and emits `chunkReady` and `chunkPlaybackStarted`.
6. `SpeechAnimator` holds the timeline and drives the mouth.
7. `AvatarController` maps each viseme to shape weights and pushes
   them to the renderer.

The viseme clock evaluates the timeline as a function of time. Each
viseme holds at full weight for most of its duration, then eases
into the next viseme over a short blend window. That is
coarticulation, and it is what keeps a held consonant from reading
as a sustained pose.

HeadTTS reconnects if the WebSocket drops. Text handed to
`enqueueSentence` while the socket is down is queued rather than
dropped, and flushed once the socket comes back.

## The codebase

- **Normal** — `MainWindow`, `DocumentManager`, `DocumentArea`,
  `TextEdit`, `TextWidget`, `ChatWidget`, `EditSession`,
  `EditPlanner`, `EditCommand`.
- **Overseer** — `OverseerPage`, `OverseerWidget`, `ConductorQueue`,
  `ConductorRoster`, `DependencyGraph`, `ConductorBoard`,
  `ConductorDock`, `FileAgent`, `Workstation`, `TranscriptPanel`,
  `NotificationService`.
- **Assistant** — `LoreAssistant`, `AssistantProfile`,
  `AssistantMemory`, `MemoryIndex`, `AssistantActivity`,
  `AssistantToolRegistry`, `SpeechAnimator`.
- **Avatar** — `AvatarWidget`, `AvatarSurface`, `AvatarRenderNode`,
  `AvatarRenderer`, `AvatarController`, `AvatarMeshLoader`,
  `VisemeTable`.
- **Inference** — `InferenceService`, `LlmClient`, `LlamaManager`,
  `TtsManager`, `NemoTranscriber`.
- **Ingest** — `IngestRegistry`, `IngestService`, `NoteWriter`,
  extractors for PDF, DOCX, PPTX, EPUB, HTML.
- **Media** — `MediaPane`, `MediaKind`, `MediaSettings`.
- **Voice** — `SpeechController`, `VoiceCommandRegistry`,
  `DictateCommand`, `LiveDictateCommand`, `ReadAloudCommand`,
  `SpeechPanel`, `AudioRecorder`.
- **Diagrams** — `GraphvizRenderer`, `PlantUmlRenderer`,
  `MermaidRenderer`, `SvgThemer`, `DiagramDocument`, `DiagramView`.
- **Themes** — `ThemeRegistry`, `ThemeTokens`, `ThemeManager`.

Deeper detail: `docs/Architecture.md`, `docs/Design Document.md`,
`docs/Theme Design Principles.md`, `docs/Create New Themes.md`,
`docs/Assistant.md`, `docs/Agents.md`, `docs/Avatar.md`,
`docs/Voice.md`.

## What I expect

- Read every file you were given, in full, before replying.
- Ask for the minimum set that discriminates between hypotheses.
- Say plainly when a hypothesis is wrong.
- Do not stack patches on unseen code.
- Do not use "just" to minimise a real change.
- Do not offer work you cannot verify.
- **Always return full files.** Not negotiable, not conditional on
  length.
- **Stop and ask before calling an unseen function.**
- **Do not silently duplicate a value across layers.** If a camera
  value or a widget offset seems to live in two places, that is the
  bug. Find the single source of truth.
