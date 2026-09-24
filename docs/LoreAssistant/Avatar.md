# The Avatar

The avatar is a Reallusion Character Creator Base figure, exported to
glTF as `Lore.glb`, skinned and animated at runtime, and drawn with a
custom OpenGL renderer.

It is not a Qt Quick 3D scene graph model. The Qt Quick 3D elements
for models, skeletons, and morph targets are not used, because Qt 6.4
caps morph targets at eight and this model has 148 on the face mesh.
The scene graph is used only as a host for the GL context and as a
place to hook the draw.

## The layers

- `AvatarWidget` is a `QQuickWidget`. It hosts the QML scene, owns the
  `AvatarController`, and handles the resize and move grips in its own
  mouse events. It does not forward mouse events to QML for the grips.
- `AvatarOverlay.qml` is the scene. It contains one `AvatarSurface`
  and a set of visual-only rectangles for the grip affordances. There
  is no camera input in this version.
- `AvatarSurface` is a `QQuickItem` with a camera. It owns the
  projection and view matrices and hands them to the render node each
  frame. It exposes camera properties and invokable methods to QML.
- `AvatarRenderNode` is a `QSGRenderNode`. It is the correct hook for
  drawing inside a `QQuickWidget`. It owns the `AvatarRenderer` and
  declares the GL state it changes.
- `AvatarRenderer` is the GL pipeline. It owns the shader program, the
  VAOs, the textures, and the per-frame draw.
- `AvatarController` owns ozz, the skeleton, the animation clips, the
  skin binding, and the per-frame skinning matrices.
- `AvatarMeshLoader` reads `Lore.glb` with tinygltf and flattens it
  into `AvatarMeshData`.
- `VisemeTable` maps Oculus viseme names onto Reallusion shape key
  indices on the face mesh.

## The model

One skin named Armature with 142 joints. 101 of those are real joints
that deform vertices; the other 41 are `_end` terminal nodes that the
glTF exporter writes at the tip of every bone chain and that deform
nothing. The ozz skeleton has 109 joints. The skin joints are matched
to ozz joints by name.

The mesh has seven pieces: body, eyes, eye occlusion, tearline,
tongue, teeth, and eyebrows. Eighteen primitives total, because some
meshes use several materials.

The face mesh `CC_Base_Body` carries 148 morph targets. Their names
are stored at the glTF mesh level, not the primitive level, and the
loader falls back to the mesh level when the primitive has none.

## The vertex buffer

Each primitive has two GL buffers.

The first is an interleaved buffer holding normal, joint indices,
joint weights, and texture coordinates. The stride is 44 bytes:
normal at 0, joints at 12, weights at 20, UV at 36.

The second holds positions only, tightly packed, one vec3 per vertex.
This split exists because the face primitive's positions are rewritten
on every viseme change, and rewriting one tightly packed buffer is one
`glBufferSubData` call instead of a strided update.

## Skinning

Skinning matrices are uploaded to a texture buffer object and read in
the vertex shader through a `samplerBuffer`. Dynamic indexing of a
uniform `mat4` array is not reliable on all GL 3.2 drivers, and radv
in particular miscompiles it. The shader fetches four texels per joint
and assembles a `mat4` from them.

The controller builds one matrix per skin joint each tick:
`modelSpace * inverseBindPose`. The inverse bind poses come from the
GLB and are used exactly as written. They are in centimetres, and so
is the ozz rest pose. The vertex positions are in metres, but the
skinning matrix is a dimensionless transform, so the unit does not
matter as long as both operands agree. Scaling the inverse bind to
match the vertices breaks the pairing and makes the mesh fly apart.

## Morph targets

The 148 morph targets on the face mesh are blended on the CPU. When
the viseme weights change, the renderer computes

    position[i] = base[i] + sum over targets of weight * delta[i]

for every face vertex and rewrites the position buffer. This is a
few thousand vertices of work per viseme change, at viseme rate, which
is negligible. The GPU path would need an attribute-indexed vertex id
that GLSL 150 does not have.

## Textures

`AvatarMeshLoader` keeps the encoded bytes of every image in the GLB.
It does not decode. `AvatarRenderer` builds a `QImage` from the bytes,
flips it vertically so that the first row of the image is the first
row of the GL texture, and uploads it as RGBA. PNG and JPEG are both
handled by `QImage`.

Each material carries the index of its base color image. Each
primitive resolves its material to that index. At draw time the
primitive binds its texture, or a 1x1 white texture if it has none,
and the fragment shader multiplies the sampled color by a uniform.

The GLB exports the body, arm, leg, and nails textures packed into
atlas bands with U offsets above 1.0, so those textures must use
`GL_REPEAT`. The head is a standalone texture in its own 0..1 UV
space. The current build uses `GL_REPEAT` for all textures; a future
pass should carry the glTF sampler's wrap mode per texture.

## The camera

The camera is a target point plus a yaw, pitch, and distance orbit
around it. The projection is rebuilt whenever any camera parameter
changes or the item's geometry changes. The model transform is
applied after the view, so a model pitch or yaw rotates the figure
inside the frame.

The widget's size and position are its own. It is not inside a
layout, so the main window's size and aspect ratio do not reach it.