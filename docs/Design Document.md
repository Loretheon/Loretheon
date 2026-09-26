# Lore — Design Principles

*Personal Knowledge Management System (PKMS)*

A PKMS is not a text editor with features bolted on. It is a system for
building and maintaining a body of knowledge over years, where the
files you write are the point and the software is a tool that stays out
of the way.

---

## 1. Semantics over syntax

The user's intent matters, not the punctuation that expresses it. A
note is a body of meaning; Markdown, plaintext, DOT, PlantUML and
Mermaid are formats that meaning happens to be written in today.

This rules out:

- Features that only work if the file is exactly one format.
- Tools that refuse to help because the syntax is unfamiliar.
- Lock-in to one representation of an idea.

This forces:

- A single editing core that treats formats as dialects, not as
  separate products.
- An AI layer that reads intent from the whole document, not from a
  regex over its first line.
- Every assistant feature being useful in every supported format.

## 2. Your files are yours

Every note Lore manages lives on your disk, in an ordinary directory,
in an ordinary format. No proprietary container, no cloud-only
representation, no export step.

This means:

- **Leave at any time.** Close the app; the folder is still a folder.
- **Use other tools.** `grep`, `git`, `rsync`, `vim`, a phone editor —
  they all see the same files.
- **Back up normally.** A copy of the folder is a complete copy of
  your knowledge base.
- **No silent transformation.** If Lore rewrites a file, the user
  asked for it, and the previous contents were readable.

## 3. Free and open source, bring your own key

Lore is FOSS. Every line is inspectable, every behaviour is changeable,
no telemetry, no phoning home.

Inference is **BYOK** — bring your own key. You choose the provider,
you own the relationship, you pay the provider directly. Lore never
proxies your content through a middleman account. Local models are a
first-class path, not a fallback.

This rules out:

- Any feature that only works if you use a specific vendor.
- Any feature that requires an account with a service Lore operates.
- Hidden inference traffic.

## 4. Two modes, one system

Lore has two ways to work, and they are speeds of the same thing, not
two products.

- **Normal mode** is the traditional note-taking ecosystem. You open a
  file, you edit it, and — if you want — you ask an LLM to make
  structured edits to what is open.
- **Overseer mode** is a workflow. You describe a body of work in
  natural language. A conductor turns each request into a card on a
  kanban board, delegates the card to the right worker, and the worker
  produces files. You supervise; you do not type every character.

The files are the same. The folder is the same. The themes are the
same. Only the pace changes.

## 5. Graphing languages are first-class

Markdown and plaintext cover prose. DOT, PlantUML and Mermaid cover
structure — hierarchies, flows, state machines, relationships between
ideas. A PKMS that only stores prose misses half of what a person
knows.

So diagrams are not attachments. They are notes. They are edited,
rendered, version-controlled, and reasoned about like any other file.

## 6. AI as a collaborator, not a gatekeeper

The LLM helps. It does not decide. Every AI action is visible, every
change is reviewable, and the user can always do the same thing by
hand.

- Structured edits are **plans** the user approves, not silent writes.
- Agent work lands on a board where the user can watch, cancel, or
  reject it.
- Memory is proposed, not assumed.
- Nothing the AI does is unrecoverable.

## 7. Cost is the user's problem only if the user wants it to be

Overseer mode is built to minimise cost and maximise output. Requests
are routed to the cheapest worker that can do the job. Independent work
is parallelised; dependent work is serialised. Files currently being
written are not handed to a second agent. The system does not burn
tokens on work it can route.

This is a design goal, not an optimisation pass. Any change that
increases token spend without a corresponding increase in quality is a
regression.

## 8. Escape at any time

No feature is only reachable through Lore. No format only renders in
Lore. No directory only works inside a session. If the user closes the
app in frustration, the only thing they lose is the app.

---

*These principles are the tie-breakers. When two features conflict,
the one that better serves the principle above wins.*