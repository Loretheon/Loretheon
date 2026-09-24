Here is a fresh-chat onboarding you can paste. It is written to stand on its own for the specific work you are in the middle of — getting the CC Base character to render — and it carries the project's working rules.

---

## Lore — assistant avatar rendering, work in progress

You are picking up an in-progress session on **Lore**, a Qt6/C++ Personal Knowledge Management System. This brief is scoped to **one task**: getting a 3D assistant character to render and animate in the corner of the main window. Read it in full before replying. Then ask for files.

---

## How we work

The full working rules are in `docs/LLM onboarding.md`. The parts that matter most here:

- **Full files, always.** Every reply that touches a file returns the complete file. No diffs, no fragments, no line ranges. A one-line change still returns the whole file. Length is not a reason to abbreviate.
- **Never guess an API.** If a signature, field, or return type is unknown, ask for the header via `xclip`. Stop before writing a call to a function you have not seen. A wrong guess costs a build; asking costs one command.
- **Ask via xclip.** When you need files, give one command that dumps to the clipboard. Debian 12, `xclip` assumed. Example:

  ```
  find ./include ./src -type f \( \
    -name 'Foo.h' -o -name 'Foo.cpp' \
  \) -not -path './cmake-build-*' \
  -print0 | xargs -0 -I{} sh -c 'echo "===== {} ====="; cat "{}"; echo' \
  | tee /tmp/deps.txt | xclip -selection clipboard
  ```

- **Diagnose, don't shotgun.** One hypothesis at a time. State what would confirm or refute it. Ask for the minimum.
- **Short.** A few sentences, then the code.
- **One change at a time.** Verify before the next.
- **Do not stack patches on unseen code.** If you have not read a file, ask for it.

Two things to be careful about, learned the hard way in the previous session:

- **Do not guess at Qt 6.4 vs newer Qt APIs.** The project is on **Qt 6.4.2** (Debian packages). Several Qt 3D APIs that exist in 6.6+ do not exist in 6.4 — `QQuick3DGeometry::setTargetData`, `addTargetAttribute`, unlimited morph targets, `QWebSocket::errorOccurred`. Check the installed version before writing a call. If in doubt, ask for the header from `/usr/include/x86_64-linux-gnu/qt6/`.
- **Do not silently duplicate a value across layers.** The previous session lost a lot of time to camera values living in three places and overriding each other. There is now a single source of truth for the camera (the QML file). Keep it that way.

---

## What Lore is

A PKMS. Ordinary files in ordinary directories, Markdown and plaintext and diagram languages. Two modes — Normal (an editor with LLM-assisted edits) and Overseer (a conductor with a roster of agents). A local-first inference stack. Everything the user creates is a file they can leave with.

The assistant is a persistent character named **Lore**. She lives in the corner of the main window, speaks, listens, watches what the user does, and remembers across sessions. She is not a chatbot in a panel. She is a resident.

Her one overriding design rule: **she can only make her own restrictions tighter, never looser.** Every setting she can write is directional. Only the user can loosen a restriction, via the settings panel. This is enforced in `Settings::setAssistantFieldRestricted`. Do not weaken it.

---

## The stack, in full

**Language and framework.** C++23, Qt 6.4.2, CMake 3.23+ with Ninja. Widgets and Qt Quick 3D both in use. Tests are ad hoc; the app is the test.

**3D.** OpenGL 3.2 core profile through `QOpenGLFunctions_3_2_Core`. The character is drawn by a custom renderer, not by the Qt Quick 3D scene graph — the scene graph is only used as a host for the GL context, and the draw happens in a `QSGRenderNode` subclass. Qt Quick 3D's own model, skeleton, and morph target elements are **not** used, because 6.4 caps morph targets at 8 and the model has 148.

**Skeleton animation.** [ozz-animation](https://github.com/guillaumeblanc/ozz-animation), MIT, built as a submodule at `external/ozz-animation`. It is renderer-agnostic; it produces joint matrices and does not draw. The offline tool `gltf2ozz` was built once into `external/ozz-animation/build-tools` and is not part of the app build.

**Model.** A Reallusion Character Creator Base character, exported from Blender as `Lore.glb`. Loaded at runtime with [tinygltf](https://github.com/syoyo/tinygltf) pinned to **v2.9.0**, header-only mode. The model has one skin named "Armature" with 101 joints, and 1969 morph targets across seven meshes. The GLB is embedded in the Qt resource system at `:/avatar/ccbase/Lore.glb`.

**Text to speech.** A Node.js server called **HeadTTS**, run as a child process from `TtsManager`. It runs the timestamped Kokoro ONNX model and returns audio plus Oculus 15-code visemes with per-phoneme timing over a WebSocket. The TTS container that was used before HeadTTS is gone. `TtsManager` lives in a separate library, `QF-ML`, at `external/QF-ML/`, and Lore consumes it through `InferenceService`.

**Speech to text.** `NemoTranscriber` inside QF-ML, running a NeMo ASR model. Not currently wired to the assistant.

**Embeddings and search.** `EmbeddingModel` in QF-ML, all-MiniLM-L6-v2 INT8, 384 dimensions. `ScopeIndex` for the notes vault, `MemoryIndex` for the assistant's own memory. Not currently wired to the avatar.

**Character model pipeline.** The character was authored as a Reallusion CC Base FBX, retargeted in Blender, exported to `Lore.glb`. The `.ozz` skeleton and clip files were produced by `gltf2ozz` from the GLB and are embedded alongside it. Both are in `resources/avatar/ccbase/`.

---

## Where we are, in the current task

**Goal.** Get the CC Base character to render upright, at the right size, in the assistant widget, and then animate.

**What is working.**

- The GLB loads. 7 meshes, 18 primitives, 1969 morph targets, 1 skin with 101 joints.
- The skin binds. All 101 skin joints match ozz joints by name.
- The ozz skeleton loads. 109 joints. 10 clips (idle, walk, walk_left, walk_right, run, run_left, run_right, turn_left, turn_right, jump) load with sensible durations.
- The custom GL renderer draws. `QSGRenderNode` is in place, all 18 primitives are drawn every frame, `glError: 0`.
- The camera orbits with left-drag. The QML `MouseArea` handles the input. Right-drag and middle-drag are being wired to pan; the previous session found that `QQuickWidget` does not reliably forward non-left buttons to QML from a widget-level `mousePressEvent` override, so the overrides were removed from `AvatarWidget` and the input now lives entirely in QML.
- A per-mesh color diagnostic distinguishes the parts on screen: body skin-tone, eyes blue, teeth white, eyebrows dark.

**What was recently fixed.**

- The GLB stores vertex positions in metres and inverse bind poses in centimetres, which is a Blender export inconsistency. `AvatarController::buildSkinBindings` scales the translation column of every inverse bind matrix by 0.01 to reconcile. Confirmed by reading the raw GLB bytes.
- `AvatarRenderer::render` was uploading uniforms before `glUseProgram`, which silently no-ops and can set `GL_INVALID_OPERATION`. Reordered.
- The vertex shader clamps joint indices to 127 so a bad index cannot read outside the `uSkinning[128]` uniform array.

**What is not yet resolved.**

- The character's default orientation. The GLB bounds are `x: 0.588, y: 0.87, z: 1.7`, which suggests a Z-up character. A `modelPitch: -90.0` rotation in the QML has been the most promising correction, but the previous session had not confirmed the final values before moving on to the input work.
- The default camera framing. Distance and target height have been tuned by feel, not yet settled.
- The widget's position and size are not saving and reloading cleanly. The suspected cause is that `MainWindow::buildAvatarOverlay` runs before `MainWindow::setGeometry` in the constructor, so the window size used to compute the bottom-right offset at load time is not the final size. Not fixed.
- Idle animation is deliberately not playing. The controller starts but does not call `playClip`. The bind pose is drawn. When no clip is playing, `AvatarController::start` must hand the initial identity skinning matrices to the surface or the shader reads a zero matrix and the mesh collapses.
- Textures are not applied. Every primitive is drawn in a flat color per mesh. Extracting the PNGs from the GLB and uploading them as GL textures is the next major visual step after the pose is settled.
- Morph targets are not driven. The 148 face shape keys on `CC_Base_Body` are loaded but not uploaded or blended. The viseme pipeline (from HeadTTS) and the shape key pipeline are not connected.

---

## The files in play

**The avatar subsystem lives under `include/avatar/` and `src/avatar/`.**

- `AvatarConfig.h` — the widget size and bounds that `MainWindow` uses to position the widget. Camera values were moved out of here to QML and should not come back.
- `AvatarWidget.h/.cpp` — a `QQuickWidget` that hosts the QML scene. Owns an `AvatarController`. Has four corner resize grips and four edge move grips. Does not override mouse events, because doing so stops right-click reaching the QML scene.
- `AvatarResizeGrip.h/.cpp` — corner grips. Left-drag resizes.
- `AvatarMoveGrip.h/.cpp` — edge grips. Right-drag moves the widget. Recently added.
- `AvatarSurface.h/.cpp` — a `QQuickItem` that hosts the renderer. Owns a `AvatarRenderNode`. Exposes camera properties (`cameraDistance`, `cameraYaw`, `cameraPitch`, `cameraFov`, `targetX/Y/Z`) and model transform properties (`modelScale`, `modelYaw`, `modelPitch`, `modelRoll`). Has `Q_INVOKABLE` methods `orbitBy`, `panBy`, `zoomBy`, `resetCamera` that the QML calls.
- `AvatarRenderNode.h/.cpp` — a `QSGRenderNode` subclass. The correct hook for drawing inside a `QQuickWidget`. Owns the `AvatarRenderer`. Its `changedStates()` must declare every GL state the renderer touches, or the following nodes draw with stale state.
- `AvatarRenderer.h/.cpp` — the GL pipeline. Owns the shader program, the VAOs, and the per-frame draw. One draw call per glTF primitive. No textures yet. Depth test on, depth clear per frame, cull off.
- `AvatarController.h/.cpp` — owns ozz, the `.ozz` archives, the mesh data, the skin binding, and the per-frame skinning matrices. Hands skinning matrices to the surface. Does not draw.

**Resources.**

- `resources/avatar/ccbase/resources.qrc` — the `.ozz` files plus `Lore.glb`.
- `resources/avatar/AvatarOverlay.qml` — the root scene. An `AvatarSurface`, a `MouseArea` for input, a `WheelHandler` for zoom, and a diagnostic overlay text.

**Outside the avatar subsystem, but on the path.**

- `external/QF-ML/` — the TTS, STT, LLM, and embedding library. Consumed by Lore through `InferenceService`.
- `MainWindow::buildAvatarOverlay` — constructs the `AvatarWidget`, reads stored geometry from `QSettings`, places it, connects the `geometryChanged` signal to a save.
- `resources/avatar/vita/` — the previous avatar, from a VRM. Retained as a fallback. Not used.

**Submodules.**

- `external/ozz-animation`
- `external/tinygltf` — pinned to v2.9.0.
- `external/QF-ML` — its own tree with its own submodules.

---

## The immediate next step

The previous session ended with three files just emitted for a fresh round of work:

- `include/avatar/AvatarMoveGrip.h` and `src/avatar/AvatarMoveGrip.cpp` — new files for the edge grips. They need to be added to CMake.
- `include/avatar/AvatarWidget.h` and `src/avatar/AvatarWidget.cpp` — updated to construct and position the four edge grips, and to handle the `moveBy` signal by moving the widget.
- `include/avatar/AvatarSurface.h` and `src/avatar/AvatarSurface.cpp` — updated to remove the C++ mouse overrides and expose the invokable camera methods.
- `resources/avatar/AvatarOverlay.qml` — updated to wire the mouse and wheel input through QML.

**The next concrete step is to build the tree and confirm one thing: does right-drag on the edges move the widget, and do the corner grips still resize it.**

If that compiles and works, the following steps are, in this order:

1. **Settle the model orientation and camera.** The GLB is likely Z-up. The `modelPitch: -90` correction in the QML is probably right, but the exact rotation and the exact `cameraDistance` and `targetY` values need one or two build cycles to confirm.
2. **Fix the widget geometry save and reload.** The suspected fix is to move `buildAvatarOverlay()` in the `MainWindow` constructor to after the `setGeometry` call.
3. **Textures.** Extract the PNGs from the GLB, upload them as GL textures, add a sampler to the fragment shader, and bind per-primitive textures. This is the single largest visual step.
4. **Morph targets.** Upload the face shape key deltas as vertex data, extend the vertex shader to blend them, and wire the HeadTTS viseme timeline to the shape key weights. The `AssistantTools` and `SpeechAnimator` subsystems already exist and are waiting.
5. **Re-enable idle animation.** `m_controller->playClip("idle")` in `AvatarWidget::onSurfaceReady` once the bind pose is confirmed correct.

**Ask for these files first:**

```
find ./include/avatar ./src/avatar -type f -not -path './cmake-build-*' \
-print0 | xargs -0 -I{} sh -c 'echo "===== {} ====="; cat "{}"; echo' \
| tee /tmp/avatar_all.txt | xclip -selection clipboard
```

And the QML and CMake:

```
for f in ./resources/avatar/AvatarOverlay.qml ./CMakeLists.txt \
         ./src/app/MainWindow.cpp; do
  echo "===== $f ====="
  cat "$f"
  echo
done | tee /tmp/avatar_context.txt | xclip -selection clipboard
```

Do not build until you have read all of them. The previous session wasted time on `glError: 1282` because a uniform upload was happening before `glUseProgram` and the log was not read carefully. Read the log. Ask for the file. One change at a time.