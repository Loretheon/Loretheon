# Agents and Tools

The LLM in Lore can call tools. A tool is a named, JSON-described
action the model can request. The model never executes anything
directly: it emits a structured request, the application runs the
tool, and the result goes back into the conversation.

There are two tool registries in the tree. They have different
contexts because they serve different callers.

## `agent::Tool` — the Overseer registry

Used by the Overseer and the retrieval loop. The context carries:

- the session folder and output folder
- the focused file path, the focused document, the focused editor
- callbacks for opening and closing files
- a callback for requesting a review of an edit
- a callback for requesting a scoped edit

The tools registered here operate on files and on the editor:
`list_directory`, `read_file`, `read_notes_file`, `write_file`,
`create_directory`, `open_file`, `close_file`, and the two memory
proposal tools.

## `assistant::AssistantTool` — the assistant registry

Used by the assistant. The context is different because the assistant
has access to different things: the inference service, the search
service, the assistant's own memory and profile, the avatar, and the
recent activity stream. The tools here are:

    search_notes       search the user's notes by meaning
    open_file          open a note in the editor
    insert_text        insert text at the cursor
    remember_fact      propose a fact for the memory tree
    change_setting     change one of the assistant's own settings
    speak              speak a line aloud
    set_expression     change the avatar's facial expression

Each tool declares its name, description, category, whether it is
destructive, and a JSON schema for its arguments. The registry hands
the whole set of schemas to the LLM in the request.

## Destructive tools

`insert_text` and `change_setting` are marked destructive. When a
destructive tool is about to run, the context's `requestReview`
callback is invoked with a title and a short body, and the tool waits
for approval. If the user declines, the tool returns successfully
with a message saying the action was declined. The LLM sees the
result and can proceed.

`change_setting` has a second guard. The setting layer itself refuses
any write that would loosen a restriction. The assistant can only
ever make her own restrictions tighter. If the user asks her to
loosen one, she is required to refuse and tell the user to do it in
the settings panel. This is enforced in the settings code, not in the
tool, so a bug in the tool cannot bypass it.

## Structured output

Some model calls need a fixed response shape rather than free text.
`StructuredSchema` wraps a small JSON schema and produces the exact
object to pass as the `response_format` argument to
`InferenceService::sendChatRequest`. `StructuredCall` runs a two-
message request with a schema attached, accumulates the streamed
reply, parses it as JSON, and hands the result to a callback exactly
once.

The schemas that ship with the application live in
`StructuredSchemaRegistry`. Currently there is one: `sufficiency`,
which judges whether a set of notes contains enough information to
answer a question. It returns:

    {"sufficient": true|false, "reason": "..."}

## The LLM

The model itself is reached through `InferenceService`, which
dispatches to one of two backends:

- a local `llama.cpp` server started by `LlamaManager` in a Docker
  container, chosen by backend (Vulkan, ROCm, CUDA, Intel, or CPU)
- a remote OpenAI-compatible endpoint, configured with an endpoint,
  a model id, an auth type, and optionally an API key

Both paths go through `LlmClient`, which speaks server-sent events and
emits deltas, tool calls, and errors as signals. A session id can be
attached to a request. On a remote provider that supports sticky
routing, all requests with the same session id land on the same
endpoint and share its prompt cache.

The model is chosen in the settings panel. Local models are managed by
`ModelManager`, which can search Hugging Face, download a variant, and
select it. Remote models are configured by endpoint and model id.