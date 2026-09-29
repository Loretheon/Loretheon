# The Assistant

The assistant is a persistent character named Lore. She is not a
chat panel. She watches what the user does, speaks, listens, and
remembers across sessions.

She is owned by `LoreAssistant`, which is constructed once by
`MainWindow` and lives for the duration of the application.

`LoreAssistant` has no visible surface of its own. Its screen is
`AssistantShell`, a full-window page with no window chrome and no
close control. The shell is the assistant's entire presence: a
quiet header, a chat view, a mind map, and a composer.

## Two shells

The application has two top-level screens, stacked in `MainWindow`:

    AssistantShell   the assistant
    WorkspacePage    Normal, Overseer, Search

The application opens on the assistant shell. `Escape` leaves it and
lands in the workspace. `Ctrl+Space` returns. There is no button on
either screen for crossing between them. The shell is not a mode of
the workspace; the workspace is not a mode of the shell. They are
peers.

The keyboard is the entire navigation. `Escape` from the shell, any
keyboard shortcut from the workspace. Nothing else.

## What owns what

`LoreAssistant` is the conductor. It holds:

- an `AssistantProfile` — the always-in-context knowledge
- an `AssistantMemory` — the durable, searchable record
- a `MemoryIndex` — a private semantic index over the memory tree
- an `AssistantActivity` — the user's action stream
- an `AssistantToolRegistry` — the tools she can call
- a `SpeechAnimator` — the viseme clock that drives the mouth
- a job cache — every background job, keyed by id

`LoreAssistant` owns none of the application objects it is given.
`InferenceService`, `AvatarWidget`, `DocumentManager`,
`SearchService`, `OverseerSessionManager`, `NotePromoter`, and
`ScopeIndex` are passed in through `Config` and held as raw pointers
for the assistant's lifetime. This is deliberate: the assistant does
not reach into `MainWindow`.

`AssistantShell` owns the visible tree of the conversation — a
`ChatTree` of `ChatNode`s, rendered as `ChatNodeWidget`s inside a
scroll area. The shell is the only widget that subscribes to
`LoreAssistant`'s reply, job, and status signals. `MainWindow` wires
the shell and the assistant together and then steps out of the way.

## What happens on startup

`MainWindow` constructs the avatar first, so the `AvatarWidget`
pointer is valid. It then constructs `LoreAssistant` with a `Config`
that carries the inference service, the avatar, the document manager,
the document area, the search service, the Overseer session manager,
the note promoter, the scope index, the notes root, and the assistant
root path. It hands the assistant to `AssistantShell`, hands the
speech controller to the shell as well, and calls `start()`.

`start()` does five things:

1. Creates the assistant root directory if it does not exist.
2. Loads the profile. Missing profile files are created with defaults.
3. Ensures the memory tree exists. Loads the memory index if there is
   one on disk; if not, builds one.
4. Installs the tools into the registry.
5. Starts the activity batch timer.

The greeting is not sent from `start()`. `MainWindow` listens for
`InferenceService::ttsReady` and speaks the first line when the TTS
child process is actually connected. A fixed delay races the child
process and the sentence is dropped if the socket is not yet open.

## The profile

Three small files under the assistant root:

    identity.md   who she is
    user.md       what she knows about the user
    self.md       what she knows about herself

These are read at startup and on every change. They are put into the
system prompt verbatim, on every turn. She does not search for them.
The user can edit them directly, and can ask her to rewrite them.

## The memory tree

Two shapes, both under `memories/`:

    memories/sessions/YYYY-MM-DD-HHMM.md
        one file per session, written when the session ends

    memories/topics/<slug>.md
        one file per topic, appended to across sessions

Recall is semantic. `AssistantMemory::recall` queries the memory
index and returns the bodies of the top matches. This is what makes
the assistant remember without being asked.

Facts that never grow — the user's name, a preference, a standing
fact — do not go here. They go in the profile, where they are always
in context. The memory tree is for things that accumulate.

## The activity stream

`AssistantActivity` records every meaningful user action as a short
text event: file opened, file saved, mode switched, search run,
message sent. The events are delivered in batches on a timer, thirty
seconds by default. Most batches produce nothing. The stream is not
persisted. Only the assistant's summaries survive, in the session
memory file.

## Saying something

`LoreAssistant::say(text)` hands the text to
`InferenceService::speak`. The inference service queues it in
`TtsManager`, which synthesises it and plays it. The mouth follows
automatically because `SpeechAnimator` is subscribed to the playback
signal. See `docs/Voice.md`.

## The turn

`handleUserMessage` starts one turn. It builds the messages array —
the system prompt, the profile, the memory recall, any pending
background results, and the user's message — and sends it with the
tool schemas attached.

The reply streams back through `llmDelta`. The first delta creates a
reply node; every subsequent delta appends to the same node. A turn
is not finished until the model stops calling tools or the tool
round limit is reached.

Tool calls arrive through `llmToolCalls`. Each call is dispatched
through the registry. Tools that return a job marker are intercepted:
the conductor starts the job, records it in the job cache, and
returns the job id to the model. The model acknowledges and moves on.

`finishTurn` ends the turn. The reply node is not cleared. It is
cleared when the user sends the next message. That way every delta
in one exchange, including deltas after a tool round, lands in the
same node.

## Jobs and the cache

Anything the assistant does that reaches outside the conversation —
a search, a delegate, a promote — is a job. A job has a stable id,
a state, and a result. It runs in the background. The conversation
is not blocked.

The result is stored in `m_jobs` on `LoreAssistant`, keyed by id.
The model is not told the result automatically. It can call
`read_job` to read it. If the job is still running, `read_job` waits.
The user can abort the wait with the abort control on the job node
or with the global abort button.

`abortJob` cancels a single job and marks it `Cancelled`. `abortAll`
cancels every non-terminal job and aborts the current turn.

## The shell

`AssistantShell` is a single page, laid out top to bottom:

    header    Lore              status              policy
    tabs      Chat | Mind
    body      chat scroll       | mind map
    composer  dictate live read | input | abort | send

The header is quiet: the name in small letterspaced caps on the
left, a short status line, a policy dropdown on the right. The
policy dropdown chooses how background results arrive — pasted into
the chat, fed into the next turn's prompt, appended to the next user
message, or chosen automatically.

The tabs switch the body between the chat view and the mind map.
The composer is a single rounded input with the speech controls on
the left, the message field in the middle, and Send on the right.

A 96-pixel avatar floats in the shell, outside the layout. It is
not part of the conversation. Its position and its size are the
user's. Left-drag orbits the camera. Right-drag moves the widget
itself. Scroll resizes the widget around its centre, clamped to the
configured min and max. Resize handles at the corners and edges of
the widget do the same with a corner-drag. The widget is always
square, so the render viewport's aspect ratio and the projection's
aspect ratio stay in agreement. No position or size is persisted
between sessions; the widget starts centred on the shell and at its
default size every time.

## The chat

The chat view renders the conversation as a flat sequence of
message cards, centred and capped at a readable column width. Each
card is a `ChatNodeWidget` bound to one `ChatNode` in the tree.

A user message is a card with a filled background. An assistant
reply is a card with a lighter background. Job, status, and error
nodes produced during a reply are children of that reply and appear
as nested cards under it, opened and closed by a small arrow rail
on the right.

The tree is not a document tree. It is a simple parent/child model
backed by `ChatTree`. A reply can have many children; a child can
have children of its own. Every node is addressable by id and
updated in place as its state changes.

When the tree is empty, a hero block appears at the top of the chat
with a one-line title and a one-line description. It disappears the
first time a message is sent.

## The mind map

The Mind tab renders what the assistant knows as a graph. Three
hubs sit around a central root:

    Profile     identity.md, user.md, self.md
    Memory      topics under memories/topics, sessions under
                memories/sessions
    Overseer    one node per session in OverseerSessionManager,
                each carrying the session description

Profile files render as full markdown cards. They use the same
renderer as the rest of the application: the node holds a
`QTextDocument`, sizes itself from the document's extent, and draws
via the document layout. The markdown goes through `setMarkdown`,
then through `setHtml` so the theme's stylesheet applies. The card
grows as tall as its content needs.

Memory topics expand to show their facts. Each fact is a small
node carrying its timestamp header and its text. Sessions render as
single-line nodes with their title and first paragraph.

Edges are cubic beziers between parent and child anchors, drawn in
the scene's foreground so they sit under the nodes. Cross-links —
file-to-file references discovered by scanning `[[...]]` patterns
in the body — draw as dashed arcs between peers, not as part of the
tree.

Layout is a force-directed simulation. Every node repels every
other; every edge is a spring. The simulation runs on a timer,
cools by displacement limiting, and settles. Positions are written
to `memories/.mindmap.json` on settle. On subsequent opens, the
cached positions are loaded and pinned, and only new nodes get a
short incremental settle. The graph is spatially stable across
sessions: it is a map, not a re-simulation.

Hover focuses a branch — the hovered node's path to the root and
its subtree stay bright, everything else dims. Click opens the
underlying file, or opens the Overseer session if the node is an
Overseer session. Drag any node to pin it in place; the position is
written to the cache on release. Drag empty space to pan. Scroll to
zoom under the cursor. Right-click shows a small menu: Refresh,
Re-layout, Fit to view.

Refresh rebuilds the node set from disk, keeps the pinned positions
of nodes that still exist, seeds new nodes near their parents,
settles just the new ones, and saves. Re-layout clears all pins,
re-seeds from the hub geometry, and runs the full simulation.

The graph reads the assistant root when the tab is opened. It is
rebuilt when the Overseer session list changes, when the user asks
for a refresh from the context menu, and when `LoreAssistant`
signals that the knowledge tree has changed.

## The prompt

The system prompt is built from the profile, the memory recall, and
a fixed runtime section. The runtime section explains the tools, the
job model, the citation convention, and the failure rule. It does not
hardcode her name or her behaviour. Those come from `identity.md`,
which the user can rewrite.

## What is not built yet

The mind map watches the assistant root only on tab open and on the
explicit refresh action. A `QFileSystemWatcher` on the assistant
root would make external edits appear without a rebuild. It is
small and it is not wired.

The mind map is a viewer. The profile files, the memory topics, the
sessions, and the Overseer descriptions are all editable as plain
files, but the graph does not offer an inline editor. Editing a
node is editing its file through the editor, then refreshing.

The chat tree is lost when the application closes. Only the
assistant's own summaries survive, in the memory tree. A session
that wants to reload its chat would need a serializer for
`ChatTree`, and a place to write it — the natural choice is a sidecar
next to the session memory file.