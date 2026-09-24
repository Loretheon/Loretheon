# The Assistant

The assistant is a persistent character named Lore who lives in the
corner of the main window. She is not a chat panel. She watches what
the user does, speaks, listens, and remembers across sessions.

She is owned by `LoreAssistant`, which is constructed once by
`MainWindow` and lives for the duration of the application.

## What owns what

`LoreAssistant` is the conductor. It holds:

- an `AssistantProfile` — the always-in-context knowledge
- an `AssistantMemory` — the durable, searchable record
- a `MemoryIndex` — a private semantic index over the memory tree
- an `AssistantActivity` — the user's action stream
- an `AssistantToolRegistry` — the tools she can call
- a `SpeechAnimator` — the viseme clock that drives the mouth

`LoreAssistant` owns none of the application objects it is given.
`InferenceService`, `AvatarWidget`, `DocumentManager`, and
`SearchService` are passed in through `Config` and held as raw
pointers for the assistant's lifetime. This is deliberate: the
assistant does not reach into `MainWindow`.

## What happens on startup

`MainWindow` constructs the avatar first, so the `AvatarWidget`
pointer is valid. It then constructs `LoreAssistant` with a `Config`
that carries the inference service, the avatar, the document manager,
the search service, and the assistant root path. Then it calls
`start()`.

`start()` does five things:

1. Creates the assistant root directory if it does not exist.
2. Loads the profile. Missing profile files are created with defaults.
3. Ensures the memory tree exists. Loads the memory index if there is
   one on disk; if not, recall returns empty until it is rebuilt.
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

These are read at startup and on every change. They are small on
purpose. Anything that grows unbounded belongs in `memories/`, not
here. The user can edit them directly.

## The memory tree

Two shapes, both under `memories/`:

    memories/sessions/YYYY-MM-DD-HHMM.md
        one file per session, written when the session ends

    memories/topics/<slug>.md
        one file per topic, appended to across sessions

Memory writes are gated. The assistant proposes a fact, the user
approves it, and only then does it land on disk. The propose-and-
approve flow lives in the conductor. `AssistantMemory` only performs
the write once approval has been given.

Recall is semantic. `AssistantMemory::recall` queries the memory
index and returns the bodies of the top matches. This is what makes
the assistant remember without being asked.

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

## What is not built yet

`LoreAssistant` currently does not run the LLM loop. It constructs
the registry, installs the tools, and can be called to speak a line,
but the code that turns an activity batch or a user message into a
prompt, dispatches it, and acts on the reply is not in this class yet.
The tool registry and the tool implementations are complete and ready
to be driven; what is missing is the conductor that drives them.