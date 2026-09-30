**The Assistant**

The assistant is a persistent character named Lore. She is not a
chat panel. She watches what the user does, speaks, listens, and
remembers across sessions.

She is owned by `LoreAssistant`, which is constructed once by
`MainWindow` and lives for the duration of the application.

`LoreAssistant` has no visible surface of its own. Its screen is
`AssistantShell`, a full-window page with no window chrome and no
close control. The shell is the assistant's entire presence: a
quiet header, a chat view, a mind map, an explorer for past
conversations, and a composer.

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
- a `ChatTreeStore` — the persistence layer for the conversation
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

`start()` does six things:

1. Creates the assistant root directory if it does not exist.
2. Creates the `pastes/` directory under it if it does not exist.
3. Loads the profile. Missing profile files are created with defaults.
4. Ensures the memory tree exists. Loads the memory index if there is
   one on disk; if not, builds one.
5. Sets the chat store root.
6. Installs the tools into the registry.
7. Starts the activity batch timer and the turn watchdog.

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
The assistant can rewrite them herself through `edit_profile`, with
no user review. Changes to `identity.md` take effect on the next
turn, because `ProfileEditJob` reloads the profile when it finishes.

The three files carry markdown headings so the scoped-edit pipeline
has something to target. `identity.md` has `## Character`,
`## Voice`, `## Tools`, `## Boundaries`. `user.md` has `## Known`,
`## Preferences`, `## Notes`. `self.md` has `## History`, `## State`,
`## Notes`. A file with no headings collapses to a single
`"document"` scope and can only be edited by whole-file replacement.

## The memory tree

Two shapes, both under `memories/`:

    memories/sessions/YYYY-MM-DD-HHMM.md
        one file per session, written when the session ends

    memories/topics/<slug>.md
        one file per topic, appended to across sessions

Recall is semantic. `AssistantMemory::recall` queries the memory
index and returns the bodies of the top four matches, injected into
the system prompt. This is what makes the assistant remember without
being asked.

Facts that never grow — the user's name, a preference, a standing
fact — do not go here. They go in the profile, where they are always
in context. The memory tree is for things that accumulate.

Nothing writes to the memory tree automatically. The only writer is
`EditProfileTool` with `target=topic`, which starts a scoped-edit job
against the topic file. The model decides what is durable and where
it belongs, one tool call at a time.

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

`handleUserMessage` starts one turn. Any number of turns may be
active at once; each has its own token, its own reply node, its own
messages array, and its own tool-round budget. The user is never
blocked by an in-flight turn.

Large pastes are extracted before the turn begins. Anything over
2000 characters is written to `pastes/<id>.txt` under the assistant
root and replaced in the message with a one-line placeholder:

    [paste: 4821 chars — id a3f19c. Call read_paste("a3f19c") to
    read it.]

The model reads the body only when it calls `read_paste`. Pastes are
never indexed and never enter the recent tail.

The messages array is built as:

    system     the profile, the memory recall, the runtime section,
               and any pending background results
    recent     the last few user/assistant pairs, as real chat
               messages, capped at 4000 estimated tokens
    user       the new message

The recent tail is derived from the loaded chat, not accumulated in
memory. When the shell loads a segment — at startup, on segment
switch, on new chat — it calls `LoreAssistant::setCurrentChat(tree)`,
which walks the tree's roots in order and rebuilds the tail from the
newest user/assistant pairs that fit the budget. A restart does not
lose the conversation; the tail is reconstructed from the chat file.

The reply streams back through `llmDelta`. The first delta creates a
reply node; every subsequent delta appends to the same node. A turn
is not finished until the model stops calling tools or the tool
round limit is reached.

Tool calls arrive through `llmToolCalls`. Each call is dispatched
through the registry. Tools that return a job marker are intercepted:
the conductor starts the job, records it in the job cache, and
returns the job id to the model. The model acknowledges and moves on.

`finishTurn` ends the turn. The reply node is not cleared. It is
cleared when the user sends the next message or opens a different
chat. The tail is updated with the completed user/assistant pair.

`abandonTurn` ends a turn that never produced a terminal signal. It
is driven by a watchdog that runs every five seconds and abandons
any turn whose request is no longer active in the inference layer.

## Jobs and the cache

Anything the assistant does that reaches outside the conversation —
a search, a delegate, a promote, a profile edit — is a job. A job
has a stable id, a state, and a result. It runs in the background.
The conversation is not blocked.

The result is stored in `m_jobs` on `LoreAssistant`, keyed by id.
The model is not told the result automatically. It can call
`read_job` to read it. If the job is still running, `read_job` waits.
The user can abort the wait with the abort control on the job node
or with the global abort button.

Jobs render as children of the assistant reply that started them.
They appear as carousel pages under the reply card, one page per
job, with dots and arrows to step through them. A job's result
appears only on its own card. It is not also pasted into the reply
text.

`abortJob` cancels a single job and marks it `Cancelled`. `abortAll`
cancels every non-terminal job and abandons every active turn.

## The shell

`AssistantShell` is a single page, laid out left to right, then top
to bottom:

    explorer  a hideable left dock with the list of past chats
    header    Lore              status              New chat
    tabs      Chat | Mind
    body      chat scroll       | mind map
    composer  dictate live read | input | abort | send

The explorer dock holds every chat file under
`<assistantRoot>/chats/<YYYY-MM-DD>/<name>.md`, grouped by day. A
date header is a non-selectable item, styled as a caption rather
than a chat. Each chat is a selectable, renameable item. A `+` at
the top of the panel creates a new chat and immediately opens it
for inline rename. Right-clicking a chat offers Rename and Delete.
Selecting a chat loads it into the tree and rebuilds the recent
tail.

The header is quiet: the name in small letterspaced caps on the
left, a short status line, and a New chat button on the right.

The tabs switch the body between the chat view and the mind map.
The composer is a single rounded input with the speech controls on
the left, the message field in the middle, and Send on the right.
Send is never disabled by an in-flight turn; the composer only
greys while the shell is streaming, and the stream does not block
the input.

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
as a carousel of pages under the reply body, one page per child,
with dots and arrows to step through them. The carousel chrome is
hidden when there is only one page.

The tree is not a document tree. It is a simple parent/child model
backed by `ChatTree`. A reply can have many children; a child can
have children of its own. Every node is addressable by id and
updated in place as its state changes.

When the tree is empty, a hero block appears at the top of the chat
with a one-line title and a one-line description. It disappears the
first time a message is sent.

## The chat files

The conversation is eternal. Each chat is one file under
`<assistantRoot>/chats/<YYYY-MM-DD>/<name>.md`, where `<name>` is a
random six-hex identifier on creation and can be renamed by the
user. The file uses the same block format as the Overseer
transcript:

    ## <kind> | <ISO8601 with ms> | <json sidecar>
    <text>
    <0x1E>

The sidecar carries the node's id, parentId, state, detail, jobId,
result, error, and child ids. Order in the file is insertion order;
children are reconstructed from parentId.

A new chat is created only when the user asks for one. There is no
per-day file and no per-message file. The shell saves the tree after
every change.

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

## The tools

- `search` — starts a search job through the assistant's own
  `RetrievalLoop`. Returns a job id.
- `read_job` — reads the result of a job by id. Waits if the job is
  still running.
- `read_paste` — reads the full text of a paste by id.
- `delegate` — hands a task to the Overseer conductor. Returns a
  job id.
- `promote_note` — copies a file the worker produced into the
  notes.
- `list_notes`, `read_note`, `write_note`, `edit_note`,
  `delete_note` — operate on the user's notes folder.
- `edit_profile` — runs a scoped edit against `identity.md`,
  `user.md`, `self.md`, or a topic file. Returns a job id. No user
  review.
- `speak` — says something aloud.

## The prompt

The system prompt is built from the profile, the memory recall, the
paste convention, the job model, the tool list, and the failure
rule. It does not hardcode her name or her behaviour. Those come
from `identity.md`, which the user can rewrite.

## What is not built yet

The mind map watches the assistant root only on tab open and on the
explicit refresh action. A `QFileSystemWatcher` on the assistant
root would make external edits appear without a rebuild. It is
small and it is not wired.

The mind map is a viewer. The profile files, the memory topics, the
sessions, and the Overseer descriptions are all editable as plain
files, but the graph does not offer an inline editor. Editing a
node is editing its file through the editor, then refreshing.

The explorer dock does not watch the chats directory. A file
renamed or deleted from outside the app will not be reflected until
the app restarts or a new chat is created.

Pastes are never garbage collected. A paste that is referenced by a
chat file lives forever, which is correct, but a paste that was
created by a message that was never sent — a write that succeeded
and a turn that failed before it went out — is orphaned. A sweep
that deletes pastes not referenced by any chat file would close
that hole. It is small and it is not wired.

The chat tree has no cross-chat search. Every chat is an island.
Searching across chats would mean indexing the chat files, which
would put them into `recall` alongside the memory tree. That is a
design decision, not a mechanism, and it has not been made.