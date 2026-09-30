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

`AssistantShell` is a single page with a header, two tabs (Chat and Mind), and a composer.

The header is quiet: the name in small letterspaced caps on the left, a short status line, a policy dropdown on the right. The policy dropdown chooses how background results arrive — pasted into the chat, fed into the next turn's prompt, appended to the next user message, or chosen automatically.

The composer is a single rounded input with the speech controls on the left, the message field in the middle, and Send on the right.

An avatar floats in the shell, outside the layout. It is not part of the conversation. Its position and size are the user's: left-drag orbits the camera, right-drag moves the widget, scroll resizes the widget around its centre, resize handles at the corners and edges do the same with a corner-drag. The widget is always square so the render viewport's aspect and the projection's aspect stay in agreement. No position or size is persisted between sessions; the widget starts centred on the shell at its default size every time.

The Chat tab renders the conversation as a tree. A user message is a root node. An assistant reply is a root node. Every job, status, and error the reply produces is a child of that reply node, indented and collapsed by default.

The Mind tab renders what the assistant knows as a graph. Three hubs sit around a central root:

    Profile     identity.md, user.md, self.md
    Memory      topics under memories/topics, sessions under
                memories/sessions
    Overseer    one node per session in OverseerSessionManager,
                each carrying the session description

Profile files render as full markdown cards. They use the same renderer as the rest of the application: the node holds a `QTextDocument`, sizes itself from the document's extent, and draws via the document layout. The markdown goes through `setMarkdown`, then through `setHtml` so the theme's stylesheet applies. The card grows as tall as its content needs.

Memory topics expand to show their facts. Each fact is a small node carrying its timestamp header and its text. Sessions render as single-line nodes with their title and first paragraph.

Edges are cubic beziers between parent and child anchors, drawn in the scene's foreground. Cross-links — file-to-file references discovered by scanning `[[...]]` patterns — draw as dashed arcs between peers.

Layout is a force-directed simulation. Every node repels every other; every edge is a spring. The simulation runs on a timer, cools by displacement limiting, and settles. Positions are written to `memories/.mindmap.json` on settle. On subsequent opens the cached positions are loaded and pinned, and only new nodes get a short incremental settle. The graph is spatially stable across sessions: it is a map, not a re-simulation.

Hover focuses a branch — the hovered node's path to the root and its subtree stay bright, everything else dims. Click opens the underlying file, or opens the Overseer session if the node is an Overseer session. Drag any node to pin it in place; the position is written to the cache on release. Drag empty space to pan. Scroll to zoom under the cursor. Right-click shows a small menu: Refresh, Re-layout, Fit to view.

Refresh rebuilds the node set from disk, keeps the pinned positions of nodes that still exist, seeds new nodes near their parents, settles just the new ones, and saves. Re-layout clears all pins, re-seeds from the hub geometry, and runs the full simulation.

The graph reads the assistant root when the tab is opened. It is rebuilt when the Overseer session list changes, when the user asks for a refresh from the context menu, and when `LoreAssistant` signals that the knowledge tree has changed.

## Single sources of truth

Some values live in exactly one place and must not be duplicated:

- **The camera** lives in the QML scene (`AvatarOverlay.qml`). It is not duplicated in C++.
- **The viseme table** lives in `VisemeTable.cpp`.
- **The node id for a reply** is created by `LoreAssistant::onLlmDelta` on the first delta and reused for every later delta. The widget obeys the id; it does not create its own.
- **The session description** is written once at creation and read when choosing a session. It is never rewritten automatically.
- **The mind map cache** lives in `memories/.mindmap.json`. Node positions are written there on settle, on drag release, and on refresh. They are read on build. Nothing else writes to it.

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
