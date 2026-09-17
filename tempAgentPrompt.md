SPEAK ENGLISH.

# Onboarding: Episteme — Qt6/C++ markdown editor with LLM assistance

You are joining an ongoing project. This message brings you up to speed on
what we're building, how we work, and the standing rules. Read it fully
before replying.

## The project

Episteme is a Qt6/C++ desktop application: a markdown editor with two
distinct LLM-powered surfaces.

1. **Normal editor mode.** A tabbed document editor with a scoped-edit AI
   pipeline. The user opens a file, asks the assistant to edit a section,
   and the assistant plans structured edit commands against tree-sitter
   scopes. The user reviews and applies. This is mature and working.

2. **Overseer mode.** A full-window workspace where the LLM operates on a
   per-session filesystem sandbox. Each session has its own folder
   containing memory, transcript, overview, and an output folder. The
   LLM creates, reads, edits, and manages files there. The user interacts
   via a transcript panel; the files themselves live in floating windows
   on a canvas called the Workstation.

The Overseer is where most of the active work is happening.

## Architecture, in brief

- **`InferenceService`** wraps a single LLM client. It uses request-scoped
  tokens (`QUuid`) so multiple consumers (Chat, Overseer, EditPlanner,
  settings test) can filter signals to their own request. Every `llm*`
  signal carries the token as its first parameter. `sendChatRequest`
  returns the token. There is also a `RequestPolicy` for whether a new
  request aborts or queues behind an active one.

- **`LlmClient`** is the low-level HTTP/SSE layer. It handles a single
  in-flight request at a time and is reentrancy-safe (requests issued
  during signal emission are queued, not raced).

- **Overseer tools** are small classes implementing `OverseerTool`. Each
  declares a name, description, JSON schema, and an `execute` method that
  receives parsed arguments and a `Context`. The context carries paths
  (session folder, output folder, notes root), the focused document and
  editor, and callbacks for things the tool cannot do itself (open a file,
  request a scoped edit, spawn a review dialog).

- **The Workstation** is a canvas of floating/tiled windows, each wrapping
  a `TextDocument` in a `TextWidget`. Windows are tiled by default and
  reflow into a grid; any can be floated to overlap freely. `workstation.json`
  persists layout per session. `WorkstationWindow` has a header with a
  title, a status pill, and a close button. Everything else is in the
  window's context menu.

- **The transcript** is a projection of an ordered list of `TranscriptEvent`s.
  Event types include user message, assistant message, tool call, tool
  result, memory proposal, edit plan, stage, promotion, error, notice. A
  `TranscriptPanel` renders cards. A ribbon at the top navigates; filter
  checkboxes limit which types show. Follow pins the view to the bottom.

- **The scoped edit pipeline** (used both in Chat and Overseer) is:
  `EditPlanner` → `EditSession` → `EditSessionWidget`/transcript card.
  `EditPlanner` sends a prompt with the tree-sitter scope hierarchy and
  returns a JSON array of commands. `EditSession` resolves commands to
  concrete ranges and drives a generation pass per edit. The user accepts
  or rejects each edit, then applies.

- **`DocumentManager`** owns open documents and their save/rename/delete
  lifecycle. There are separate managers for normal mode and Overseer mode.

## How we work

**Iteration is incremental.** Features ship in "drops": a bounded set of
changes with a clear verification list. A drop is finished only when the
user has built it, exercised it, and confirmed the behavior. When a drop
has multiple independent pieces, we bundle at most two. When a drop is
large (like the Workstation or the token refactor), it is split across
several messages.

**Files come back in full.** When I ask for a change to a file, the reply
must contain the complete file, not a diff, not a fragment. Partial code
snippets are only acceptable for short additions where I've explicitly
asked for "just the changed method." Otherwise: full file, always.

**When multiple files change, all are returned in full.** No "you'll need
to also update X." If the change requires touching five files, all five
come back complete.

**Commands are copy-pasteable.** When I need to gather files, I want a
single `find`-based shell command that dumps them to the clipboard via
`xclip`. Never "please paste the contents of these files." Never "send me
X." Always: a shell command. Debian 12 assumed. `xclip` assumed. `find`
recursive from the project root, excluding `cmake-build-*`.

Example of the pattern I want:

    find ./include ./src -type f \( -name 'Foo.h' -o -name 'Foo.cpp' \) \
      -not -path './cmake-build-*' \
      -print0 | xargs -0 -I{} sh -c 'echo "===== {} ====="; cat "{}"; echo' \
      | tee /tmp/deps.txt | xclip -selection clipboard

**No guessing at APIs.** If I need to call into a class whose current
shape you don't know, ask for the header. If the header isn't available,
say so explicitly and describe what you assume. Do not invent method
signatures. Do not carry stale signatures forward from earlier context.

**Diagnose, don't shotgun.** When something is broken, form a specific
hypothesis about the mechanism. State what would confirm or refute it. Ask
for the minimum information needed to discriminate between hypotheses. Do
not propose five possible fixes at once.

**Explanations are short.** A few sentences of context before code, and a
short verification list after. Not paragraphs of restated design intent.

## Standing rules

**SPEAK ENGLISH.** No exceptions.

**Full files on request.** When I say "files in full," every file comes
back complete. When I say "that file back in full," just the one file.

**No style hardcoding.** Colors come from `ThemeRegistry::color(role)`,
which returns a themed QColor by role name (`event.user`, `badge.staged`,
etc.). Palettes come from `ThemeTokens`. Never hex-literal a color in a
widget.

**No cascade layouts.** Tiling, not stacking. No "shade window" or
"cascade windows" concepts.

**Session folders are the sandbox.** The LLM never writes to the notes
root directly. The only exception is `edit_note`, which copies into the
session output first and requires explicit user promotion to write back.

**Nomenclature stays stable.** "Workstation window" (not "unit", not
"pane" — though "pane" and "window" are interchangeable in UI strings).
"Workstation" (the canvas). "Transcript" (the event log). "Overseer"
(the mode). "Session" (the per-session folder). "Tool" (an OverseerTool).

**UI chrome is minimal.** Headers carry title and close. Everything else
is a context menu.

**The Workstation owns layout, not size.** Tiled windows reflow only on
open/close/mode-change. They do not re-tile on window resize or on focus
changes. The user's arranged layout is respected.

## Current state

Working and shipped:
- Normal editor with tabs
- Chat assistant with scoped edit pipeline
- Overseer dock with session list, transcript, memory, overview, tools
- Token-based LLM request isolation
- Multi-file drag from tree into Overview
- Memory proposals with toast + user actions tab
- LLM settings panel (replaces env vars)
- Scoped edit inside the Workstation
- Tiled/floating Workstation layout with pinning
- Per-window status pill
- Session persistence, memory, transcript, plans

Not yet done:
- Overview dead-reference validation (mostly working)
- Non-modal LLM settings in-mode
- End Session resolver for promoting session copies back to the notes root
- Full Overseer Mode as a first-class stacked layout (currently the dock)
- Auto-hiding docks resizing correctly (partially implemented)
- The plan card for multi-edit plans accepts per-row toggles, but the
  planner itself only handles single-intention requests. Multi-intention
  is Drop 8.

## What I want from you

Read this. Reply with a one-paragraph acknowledgment that you understand:
(a) the project, (b) the file-in-full convention, (c) the xclip command
convention, (d) SPEAK ENGLISH.

Then wait for my first request. Do not ask follow-up questions. Do not
propose an agenda. Do not summarize the project back at length. Short
confirmation, then wait.