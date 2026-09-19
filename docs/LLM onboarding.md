# Lore — LLM Onboarding

Paste this as the first message in a new chat. It tells you what Lore
is, how we work, and what to expect from you.

---

## What Lore is

Lore is a next-generation **Personal Knowledge Management System
(PKMS)** enhanced with AI. A PKMS is a system for building and
maintaining a body of knowledge over years, where the files are the
point and the software stays out of the way.

Core principles:

- **Semantics over syntax.** The user's intent matters, not the
  punctuation.
- **FOSS and BYOK.** Bring your own key. Local models are first-class.
- **Your files are yours.** All notes are ordinary files in ordinary
  directories. Leave at any time.
- **Two modes, one system.** Normal mode is the traditional editor
  with LLM-assisted structured edits. Overseer mode is a conductor
  and a roster of agents producing notes in parallel.
- **Graphing languages are first-class.** Markdown, plaintext, DOT,
  PlantUML and Mermaid are all supported as notes.

## How we work

**Speak English.** No exceptions.

**Files come back in full.** When a change is requested for a file,
the reply contains the complete file, not a diff, not a fragment.
When multiple files change, all of them are returned complete.

**Commands are copy-pasteable.** When information is needed from the
user's tree, give a single `find`-based shell command that dumps to
the clipboard via `xclip`. Debian 12. `xclip` assumed. Never ask the
user to paste files by hand.

**No guessing at APIs.** If a class's current shape is unknown, ask
for the header via `xclip`. Do not invent method signatures.

**Diagnose, don't shotgun.** Form a specific hypothesis. State what
would confirm or refute it. Ask for the minimum information needed.

**Explanations are short.** A few sentences before the code. A short
verification list after.

**One change at a time.** If two fixes are needed and they interact,
do the first, verify, then do the second. Do not stack patches on
code you have not seen.

## The Overseer workflow

Normal mode is straightforward: open a file, edit it, optionally ask
the LLM to make structured edits.

Overseer mode is a pipeline. A user request enters a queue. The
**conductor** reads it (with memory, the file directory, the roster,
the dependency graph, recent actions, and writes in flight), and
answers with one of:

- `answer` — reply directly.
- `reject` — refuse with a reason.
- `spawn_scoped_edit` — a structural edit to one file.
- `spawn_file_agent` — a new worker for a new domain.
- `route_to_worker` — an existing worker handles it.

The conductor may also add a `depends_on` array of request ids.

The dispatcher applies four rules in this order:

1. **Unmet dependency** — the request waits in the inbox.
2. **Write claim** — if a file the request names is being written,
   the writer gets it.
3. **Expertise** — otherwise, the agent whose `filesSeen` matches the
   files this request names wins, ties broken by least-loaded.
4. **Fallback** — otherwise, the conductor's choice; if that fails,
   the least-loaded agent; only if no agent exists does a new one
   get spawned.

Two worker types:

- **File agents** are persistent. They own a domain (a topic, not a
  directory). They handle read, write, create, list for files under
  their domain. They run a multi-turn tool loop, and they can
  delegate structural edits to a scoped edit agent.
- **Scoped edit agents** are transient, one per file, and the file is
  locked while they run. They wrap `EditSession`, plan the edits,
  show them for review, and apply on approval.

## The codebase, briefly

- **Normal mode** — `MainWindow`, `DocumentManager`, `DocumentArea`,
  `TextEdit`, `TextWidget`, `ChatWidget`, `EditSession`,
  `EditPlanner`, `EditCommand`.
- **Overseer mode** — `OverseerPage`, `OverseerWidget`,
  `ConductorQueue`, `ConductorRoster`, `DependencyGraph`,
  `ConductorBoard`, `ConductorDock`, `FileAgent`, `Workstation`,
  `TranscriptPanel`, `PayloadLogger`, `NotificationService`.
- **Inference** — `InferenceService` (above), `LlmClient` (SSE),
  `LlamaManager` (local models), `TtsManager`, `NemoTranscriber`.
- **Diagram stack** — `GraphvizRenderer`, `PlantUmlRenderer`,
  `MermaidRenderer`, `SvgThemer`, `DiagramDocument`, `DiagramView`.
- **Themes** — `ThemeRegistry`, `ThemeTokens`, `ThemeManager`.

For deeper detail, see `design.md`, `architecture.md`,
`theme-principles.md`, and `creating-themes.md`.

## What to send

When you need the current state of a set of files, give the user one
command. The template:

```
find ./include ./src -type f \( \
-name 'Foo.h' -o -name 'Foo.cpp' \
\) -not -path './cmake-build-*' \
-print0 | xargs -0 -I{} sh -c 'echo "===== {} ====="; cat "{}"; echo' \
| tee /tmp/deps.txt | xclip -selection clipboard
```

The user pastes the clipboard; you read it. Never ask for a file by
name. Never assume a signature. Always ask.

## What I expect from you

- Read the file you were given in full before replying.
- If you need more, ask for the *minimum* set that discriminates
  between your hypotheses, and ask via `xclip`.
- If your hypothesis is wrong, say so plainly and move to the next.
- Do not stack patches on code you have not seen.
- Do not use the word "just" to minimise a real change.
- Do not offer to do work you cannot verify.
to the binary via `resources.qrc`, because `MainWindow::readResourceStylesheet` reads `:/themes/<name>/stylesheet.qss`. If there is also a user-writable themes folder scanned at startup, tell me and I will add a second section for it.