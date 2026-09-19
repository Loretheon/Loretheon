# Lore — Architecture

## Shape of the application

Lore is a Qt6 desktop application. It presents two modes over one
shared core:

- **Normal mode** — the classic editor: a file tree, an editor, a
  preview/diagram pane, and an LLM chat panel that can propose
  structured edits to the open document.
- **Overseer mode** — an agent workflow: a conductor, a request queue,
  a roster of file agents, a kanban board, a workstation of open
  files, and a transcript.

Both modes share the same document manager, the same diagram
rendering stack, the same inference service, and the same theme
system. Switching modes does not change the files on disk.

```
                 ┌────────────────────────────────┐
                 │           MainWindow           │
                 │  (mode toggle, menus, toasts)  │
                 └───────────────┬────────────────┘
                                 │
              ┌──────────────────┴──────────────────┐
              │                                     │
┌──────────▼──────────┐              ┌───────────▼───────────┐
│    Normal mode      │              │     Overseer mode     │
│  (editor + chat)    │              │  (conductor + board)  │
└──────────┬──────────┘              └───────────┬───────────┘
│                                     │
└──────────────────┬──────────────────┘
│
┌──────────────────▼──────────────────┐
│   DocumentManager / DocumentArea    │
│           Workstation               │
└──────────────────┬──────────────────┘
│
┌──────────────────▼──────────────────┐
│     Diagram rendering stack         │
│  Graphviz · PlantUML · Mermaid      │
└──────────────────┬──────────────────┘
│
┌──────────────────▼──────────────────┐
│       InferenceService              │
│          └─ LlmClient (SSE)         │
└─────────────────────────────────────┘
```

## The editor core

**DocumentManager** owns the set of open documents. Every document is a
`TextDocument`, which knows its format (Markdown, PlainText, Dot,
PlantUml, Mermaid) and its file path. `DocumentArea` holds the widgets
that display them; `TextEdit` is the actual editor; `TextWidget` is the
per-document container that hosts either the editor, a preview, or a
rendered diagram.

The key point: **format is a property of the document, not of the
window**. The same `TextWidget` shows a `.md` as rendered Markdown, a
`.dot` as a rendered graph, and a `.txt` as plain text, switching
between an edit view and a view view without changing the document
model.

## The diagram stack

DOT, PlantUML and Mermaid all go through the same pipeline:

- A renderer class per language (`GraphvizRenderer`,
  `PlantUmlRenderer`, `MermaidRenderer`). Each takes source text,
  shells out to the appropriate external tool (`dot`, `plantuml`,
  `mmdc`, or an equivalent), and produces SVG.
- `SvgThemer` rewrites the SVG's colours to match the active theme
  before display.
- `DiagramDocument` holds the resulting SVG and indexes its nodes and
  edges so the UI can find what was clicked.
- `DiagramView` displays it, with zoom, pan, hover, and click-through
  to open referenced documents.

The same three-class stack is used everywhere a graph appears: normal
mode's preview pane, the graph panel in the conductor board, and the
rendered view of any `.dot`, `.puml` or `.mmd` file.

## The inference layer

**LlmClient** is the only thing that touches the network. It opens an
HTTP POST with `Accept: text/event-stream`, reads the SSE stream line
by line, and emits a `deltaReceived` signal for each content chunk.
It also emits `toolCallsReceived`, `requestFinished`, and
`requestError`. Requests are identified by a `QUuid` token; only the
caller that holds the token hears about its reply.

**InferenceService** sits one level above. It owns one `LlmClient`,
configures it (endpoint, model, auth), and exposes
`sendChatRequest(...)`. Crucially, it also assigns a **stable
session id** to every request — the file agent's id, the scoped
edit's file path, the conductor's session name. OpenRouter uses that
id as a sticky routing key, so all of an agent's requests land on the
same provider endpoint and share its prompt cache.

```
      FileAgent / EditPlanner / Conductor
                    │
                    │ sendChatRequest(messages, …, sessionId)
                    ▼
            InferenceService
                    │
                    │ sendRequest(url, model, key, sessionId)
                    ▼
               LlmClient  ──  QNetworkAccessManager  ──  HTTPS / SSE
```

## Overseer mode

The workflow is a pipeline:

```
user request
│
▼
ConductorQueue ──► Conductor ──► routing decision
│                              │
│                              ▼
│                     ┌─────────────────────┐
│                     │ route / spawn / edit│
│                     └──┬──────────────┬───┘
│                        │              │
│                        ▼              ▼
│                   FileAgent     Scoped edit agent
│                        │              │
│                        ▼              ▼
│                    tool loop       EditSession
│                  (read/write)     (plan + apply)
│                        │              │
│                        └──────┬───────┘
│                               │
▼                               ▼
DependencyGraph  ◄──── taskFinished ──── ConductorBoard
```

**ConductorQueue** holds requests in a `queue.json` file. Each request
has a state: `inbox`, `routing`, `delegated`, `awaiting`, `done`,
`failed`, `rejected`. The queue is the single source of truth for what
work exists.

**ConductorRoster** holds the list of live workers, persisted as
`roster.json`. It is what the conductor's prompt reads to decide who
exists and who is busy.

**DependencyGraph** is a directed graph persisted as a DOT file
(`dependencies.dot`). An edge A → B means "B depends on A". The
conductor can declare dependencies in its JSON response; the
dispatcher records them and defers B until A is `done`.

**ConductorBoard** is the kanban UI. Seven columns, one per state,
plus a graph panel and a roster strip at the top.

**FileAgent** is a persistent worker. It handles `read_file`,
`write_file`, `create_directory`, `list_directory` inside its domain,
and can delegate a structural edit of an existing file to a scoped
edit agent. It runs a **multi-turn loop**: after every tool call it
re-prompts the model with the tool's result, until the model says
`done` or `fail`. It remembers which files it has seen.

**Scoped edit agent** is a transient worker wrapping `EditSession`. One
per file, and the file is locked while it runs. It plans the edits,
shows them to the user, and applies them when approved.

## Routing

When a request reaches the front of the queue, the conductor reads a
prompt containing the user request, global and session memory, the
file directory, the agent roster, the graph edges, recent actions, and
writes in flight. It answers with a JSON object naming one of:
`answer`, `reject`, `spawn_scoped_edit`, `spawn_file_agent`,
`route_to_worker`. It may also add a `depends_on` array of request ids.

The dispatcher then applies four rules, in order:

1. **Unmet dependency.** If any of the request's dependencies is not
   `done`, the request goes back to `inbox`. It does not dispatch.
2. **Write claim.** If the request names a file that an agent is
   currently writing, the request goes to that writer.
3. **Expertise.** Otherwise, if any agent has the files this request
   names in its `filesSeen`, the request goes to the agent with the
   most matches, ties broken by least-loaded.
4. **Fallback.** Otherwise, honour the conductor's choice; if that
   fails, give the request to the least-loaded agent; only if no agent
   exists does the system spawn a new one.

## Memory and persistence

Every session lives in a folder under `Overseer/Sessions/<name>/`:

- `queue.json` — the request queue
- `roster.json` — the worker roster
- `dependencies.dot` — the dependency graph
- `transcript.md` — the transcript of the session
- `memory.md` — session-scoped facts
- `settings.json` — per-session toggles
- `overview.md` — the session's self-description
- `proposals.json` — pending memory proposals
- `logs/` — per-session conductor, agent and edit logs
- `output/` — the actual notes the user is writing

The output folder is the point. Everything else is bookkeeping.
