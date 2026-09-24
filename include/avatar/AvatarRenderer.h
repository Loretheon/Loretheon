#pragma once

#include "AvatarMeshData.h"

#include <QMatrix4x4>
#include <QObject>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions_3_2_Core>
#include <QVector>

#include <vector>

// The GL renderer for the avatar.
//
// Skinning matrices are uploaded to a texture buffer object and read
// in the vertex shader through a samplerBuffer, because dynamic
// indexing of a uniform mat4 array is not reliable on all GL 3.2
// drivers.
//
// Base color textures are uploaded once, at initialize, from the
// encoded bytes the loader kept. Decoding is done with QImage. One
// GL texture per AvatarTexture. A 1x1 white texture stands in for
// primitives with no base color texture, so the sampler call is
// always valid and the multiply is a no-op.
//
// Morph targets on the face mesh are blended on the CPU. The face
// primitive's position lives in its own VBO, separate from the
// interleaved normal/joint/weight buffer, so a viseme change is one
// glBufferSubData of the position region.
class AvatarRenderer : public QObject,
                       protected QOpenGLFunctions_3_2_Core {
  Q_OBJECT

public:
  explicit AvatarRenderer(QObject *parent = nullptr);
  ~AvatarRenderer() override;

  bool initialize(const AvatarMeshData &meshData);
  void destroy();

  void setSkinningMatrices(const std::vector<QMatrix4x4> &matrices);
  void setMorphWeights(const std::vector<float> &weights);

  void render(const QMatrix4x4 &viewProjection);

  bool isReady() const { return m_ready; }

private:
  struct GpuPrimitive {
    GLuint vao = 0;

    // Interleaved buffer holding normal, joint indices, joint
    // weights. Positions are not in here.
    GLuint vbo = 0;

    // Positions only, tightly packed, one vec3 per vertex. This is
    // the buffer that is rewritten when the morph weights change.
    GLuint posVbo = 0;

    GLuint ibo = 0;
    GLsizei indexCount = 0;
    int materialIndex = -1;
    int vertexCount = 0;
    int morphTargetCount = 0;

    // Index into AvatarMeshData::textures for the base color, or -1.
    int baseColorTextureIndex = -1;

    // Set only on the face mesh.
    bool isFacePrimitive = false;

    QVector<float> basePositions;
    QVector<float> morphDeltas;
  };

  struct GpuMesh {
    QString name;
    QVector<GpuPrimitive> primitives;
  };

  bool compileShaders();
  bool uploadMesh(const AvatarMesh &mesh, GpuMesh &out,
                bool captureMorphSource,
                const AvatarMeshData &meshData);
  void freeGpuData();
  void uploadSkinBuffer();
  void rebuildFacePositions();

  // Decode every AvatarTexture into a QImage and upload one GL
  // texture each. Also creates the 1x1 white fallback texture.
  bool uploadTextures(const AvatarMeshData &meshData);

  GLuint m_programId = 0;
  GLint m_uViewProj = -1;
  GLint m_uColor = -1;
  GLint m_uSkinBuffer = -1;

  GLuint m_skinTbo = 0;
  GLuint m_skinTexture = 0;
  int m_skinCapacity = 0;
  GLint m_uBaseColor = -1;
  // One GL texture per AvatarTexture, in the same order. Index is
  // the image index from the GLB.
  QVector<GLuint> m_textures;

  // 1x1 white RGBA texture, bound for primitives with no base color
  // texture so the sampler call is always valid.
  GLuint m_whiteTexture = 0;

  QVector<GpuMesh> m_meshes;

  std::vector<QMatrix4x4> m_skinningMatrices;
  std::vector<float> m_morphWeights;

  bool m_ready = false;
};