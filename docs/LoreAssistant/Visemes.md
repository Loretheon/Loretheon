# Voice and Visemes

When the assistant speaks, three things happen at once: audio plays,
the mouth moves, and the audio is timed so the two stay in sync.

## The pipeline

1. `LoreAssistant::say(text)` calls `InferenceService::speak`.
2. `InferenceService` hands the text to `TtsManager`.
3. `TtsManager` sends a `synthesize` message over a WebSocket to a
   HeadTTS server, which runs the timestamped Kokoro ONNX model.
4. HeadTTS replies with one text message carrying the viseme codes,
   their start times, and their durations, followed by one binary
   message carrying the audio samples.
5. `TtsManager` zips the three viseme arrays into a timeline and
   attaches it to an `AudioChunk`. It emits `chunkReady` as soon as
   the audio is decoded, and `chunkPlaybackStarted` when the audio
   sink actually begins playing.
6. `InferenceService` re-emits both signals.
7. `SpeechAnimator` is subscribed to `ttsChunkPlaybackStarted`. It
   holds the timeline and drives the mouth.

## The viseme codes

HeadTTS emits the Oculus 15 set:

    sil  PP  FF  TH  DD  kk  CH  SS  nn  RR  aa  E  I  O  U

The five vowels drive the visible mouth shapes. The consonant codes
drive a closed or near-closed mouth. `sil` is neutral.

## The mapping

The CC Base face mesh does not use Oculus codes as shape key names.
Its shape keys are Reallusion's own, and the ones the mouth cares about
are:

    V_Open          jaw open
    V_Wide          mouth wide
    V_Tight         mouth tight
    V_Tight_O       mouth rounded
    V_Dental_Lip    upper teeth on lower lip
    Mouth_Close     lips pressed
    Jaw_Open        jaw dropped

`VisemeTable` maps each Oculus code to one or more of those shapes at
a given weight. `aa` maps to `V_Open` at 0.90 and `Jaw_Open` at 0.55.
`PP` maps to `Mouth_Close` at 0.85. Several codes share a shape and are
differentiated by weight.

The table is built once at load time against the actual morph target
names present on the face mesh. A shape key that the export did not
carry is skipped with a warning, not an error.

## The clock

`SpeechAnimator` owns a `QElapsedTimer` and a 16 ms `QTimer`. When a
chunk starts playing, it stores the chunk's timeline, starts the
clock, and applies the first shape immediately so the mouth moves on
the same frame the audio starts.

On every tick it advances a forward index into the timeline while the
next viseme's start time has been passed, and applies that shape. When
the last viseme's end is passed, it returns the mouth to `sil` and
stops the clock.

If a chunk carries no visemes, the clock idles for that chunk and logs
one warning for the animator's lifetime.

## From shape name to pixels

`SpeechAnimator` calls `AvatarWidget::applyViseme(shape)`.
`AvatarWidget` forwards to `AvatarController::applyViseme`.

The controller zeroes the morph weight vector, looks the viseme up in
`VisemeTable`, and writes each mapped weight into the vector at its
morph target index. It then hands the vector to `AvatarSurface`, which
hands it to `AvatarRenderNode`, which hands it to `AvatarRenderer`.

The renderer recomputes the face positions from the base positions
and the weighted deltas and rewrites the position buffer. The next
frame the GPU draws the new positions.

## Why the timing is right

The visemes are timed to the audio by HeadTTS, not by the application.
Each viseme carries a start time in milliseconds relative to the start
of the chunk. The clock in `SpeechAnimator` starts when playback
starts, not when the chunk arrives. The difference matters: a chunk
can be decoded a few hundred milliseconds before the previous chunk
finishes playing, and starting the clock at decode time would put the
mouth ahead of the audio.

## Text to speech configuration

`TtsManager` starts HeadTTS as a child process on first use. The node
binary and the HeadTTS directory are baked into the library at build
time and can be overridden with the `QF_NODE_CLI` and `QF_HEADTTS_DIR`
environment variables. The default port is 8882.

The voice is set in the settings panel. The default is `af_bella`.