## What you are working on

You are working on the **assistant** inside Lore. Not the editor, not the Overseer workflow, not the ingest layer. The assistant.

The assistant is a persistent character named Lore. She is owned by `LoreAssistant`, which is constructed once by `MainWindow` and lives for the duration of the application. She has a profile, a memory tree, a private semantic index, an activity stream, a tool registry, and a job cache. She speaks, listens, and remembers.

The two source files that matter most are `include/assistant/LoreAssistant.h` and `src/assistant/LoreAssistant.cpp`. Everything else in `include/assistant/` and `src/assistant/` exists to serve them.

## Environment

- **Debian 12.**
- **Qt 6.4.2**, from the Debian packages. Not 6.6, not 6.7. APIs that exist in later Qt do not exist here. If a signature is in doubt, ask for the header from `/usr/include/x86_64-linux-gnu/qt6/`.
- **X11 with KWin.** A top-level window with a transparent background needs a compositor. KWin is one. The avatar is a top-level frameless window; if it shows a black background, the cause is compositing, not the code.
- **C++23**, built with CMake 3.23 or later. `target_include_directories` on the `Lore` target is the include path. Headers are added to the target's source list explicitly.
- **`xclip`** is assumed. When you need files, dump them to the clipboard.

## Rules

**Full files, always.** Every reply that touches a file returns the complete file. No diffs, no fragments, no line ranges. A one-line change still returns the whole file. Length is not a reason to abbreviate.

**Never guess an API.** If a signature, field, or return type is unknown, ask for the header via `xclip`. Stop before writing a call to a function you have not seen. A wrong guess costs a build; asking costs one command.

**Ask via xclip.** When you need files, give one `find` command that dumps to the clipboard. Multiple files go in one `find` with several `-o -name` clauses.

```
find ./include ./src -type f \( \
-name 'Foo.h' -o -name 'Foo.cpp' \
\) -not -path './cmake-build-*' \
-print0 | xargs -0 -I{} sh -c 'echo "===== {} ====="; cat "{}"; echo' \
| tee /tmp/deps.txt | xclip -selection clipboard
```

**Diagnose, don't shotgun.** One hypothesis at a time. State what would confirm or refute it. Ask for the minimum.

**Short.** A few sentences, then the code.

**One change at a time.** Verify before the next.

**Read the log before the next change.** The application writes its state to the terminal. A wrong build shows up there before it shows up on screen. Read it carefully before deciding what to do next.

## The shape of the assistant

`LoreAssistant` is the conductor. It holds:

- an `AssistantProfile` — the always-in-context knowledge
- an `AssistantMemory` — the durable, searchable record
- a `MemoryIndex` — a private semantic index over the memory tree
- an `AssistantActivity` — the user's action stream
- an `AssistantToolRegistry` — the tools she can call
- a `SpeechAnimator` — the viseme clock that drives the mouth
- a job cache — every background job, keyed by id

`LoreAssistant` owns none of the application objects it is given. `InferenceService`, `AvatarWidget`, `DocumentManager`, `DocumentArea`, `SearchService`, `OverseerSessionManager`, `NotePromoter`, and `ScopeIndex` are passed in through `Config` and held as raw pointers for her lifetime.

The files under `include/assistant/` and `src/assistant/`:

- **Conductor** — `LoreAssistant.h`, `LoreAssistant.cpp`
- **Profile** — `AssistantProfile.h`, `AssistantProfile.cpp`
- **Memory** — `AssistantMemory.h`, `AssistantMemory.cpp`, `MemoryIndex.h`, `MemoryIndex.cpp`
- **Activity** — `AssistantActivity.h`, `AssistantActivity.cpp`
- **Tools** — `AssistantTool.h`, `AssistantToolRegistry.h`, `AssistantToolRegistry.cpp`, `tools/AssistantTools.h`, `tools/AssistantTools.cpp`, `tools/NoteTools.h`, `tools/NoteTools.cpp`
- **Speech** — `SpeechAnimator.h`, `SpeechAnimator.cpp`
- **Widget** — `AssistantWidget.h`, `AssistantWidget.cpp`
- **Chat tree** — `ChatNode.h`, `ChatNode.cpp`, `ChatTree.h`, `ChatTree.cpp`, `ChatNodeWidget.h`, `ChatNodeWidget.cpp`
- **Mind map** — `MindMapNode.h`, `MindMapNode.cpp`, `MindMapScene.h`, `MindMapScene.cpp`, `MindMapView.h`, `MindMapView.cpp`

## The turn

`handleUserMessage` starts one turn. It builds the messages array — the system prompt, the profile, the memory recall, any pending background results, and the user's message — and sends it with the tool schemas attached.

The reply streams back through `llmDelta`. The first delta creates a reply node; every subsequent delta appends to the same node. A turn is not finished until the model stops calling tools or the tool round limit is reached.

Tool calls arrive through `llmToolCalls`. Each call is dispatched through the registry. Tools that return a job marker are intercepted: the conductor starts the job, records it in the job cache, and returns the job id to the model. The model acknowledges and moves on.

`finishTurn` ends the turn. The reply node is not cleared. It is cleared when the user sends the next message. That way every delta in one exchange, including deltas after a tool round, lands in the same node.

## Jobs and the cache

Anything the assistant does that reaches outside the conversation — a search, a delegate, a promote — is a job. A job has a stable id, a state, and a result. It runs in the background. The conversation is not blocked.

The result is stored in `m_jobs` on `LoreAssistant`, keyed by id. The model is not told the result automatically. It can call `read_job` to read it. If the job is still running, `read_job` waits. The user can abort the wait with the abort control on the job node or with the global abort button.

## The profile

Three small files under the assistant root:

    identity.md   who she is
    user.md       what she knows about the user
    self.md       what she knows about herself

These are read at startup and on every change. They are put into the system prompt verbatim, on every turn. She does not search for them. The user can edit them directly, and can ask her to rewrite them.

Anything that grows unbounded belongs in `memories/`, not here.

## The memory tree

Two shapes, both under `memories/`:

    memories/sessions/YYYY-MM-DD-HHMM.md
    memories/topics/<slug>.md

Recall is semantic. `AssistantMemory::recall` queries the memory index and returns the bodies of the top matches.

Facts that never grow — the user's name, a preference, a standing fact — do not go here. They go in the profile.

## The widget

`AssistantWidget` is a top-level window with two tabs: Chat and Mind.

The Chat tab renders the conversation as a tree. A user message is a root node. An assistant reply is a root node. Every job, status, and error the reply produces is a child of that reply node, indented and collapsed by default.

The Mind tab renders the assistant's own files as a nested graph: `identity.md`, `user.md`, `self.md`, and everything under `memories/`. Topics expand to show the facts they contain. The graph is navigated by cursor position, not clicks.

## Single sources of truth

Some values live in exactly one place and must not be duplicated:

- **The camera** lives in the QML scene (`AvatarOverlay.qml`). It is not duplicated in C++.
- **The avatar widget's size and offset** live in `QSettings`, written on move and resize.
- **The viseme table** lives in `VisemeTable.cpp`.
- **The node id for a reply** is created by `LoreAssistant::onLlmDelta` on the first delta and reused for every later delta. The widget obeys the id; it does not create its own.
- **The session description** is written once at creation and read when choosing a session. It is never rewritten automatically.

When a value seems to be wrong in more than one place, the fix is to find the duplication and remove it, not to override at each site.

## What I expect

- Read every file you were given, in full, before replying.
- Ask for the minimum set that discriminates between hypotheses.
- Say plainly when a hypothesis is wrong.
- Do not stack patches on unseen code.
- Do not use "just" to minimise a real change.
- Do not offer work you cannot verify.
- **Always return full files.** Not negotiable, not conditional on length.
- **Stop and ask before calling an unseen function.**
- **Do not silently duplicate a value across layers.**

