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

## The codebase

- **Normal** — `MainWindow`, `DocumentManager`, `DocumentArea`,
  `TextEdit`, `TextWidget`, `ChatWidget`, `EditSession`,
  `EditPlanner`, `EditCommand`.
- **Overseer** — `OverseerPage`, `OverseerWidget`, `ConductorQueue`,
  `ConductorRoster`, `DependencyGraph`, `ConductorBoard`,
  `ConductorDock`, `FileAgent`, `Workstation`, `TranscriptPanel`,
  `NotificationService`.
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
`docs/Theme Design Principles.md`, `docs/Create New Themes.md`.

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