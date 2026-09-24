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
// Morph targets on the face mesh are blended on the CPU. The face
// primitive's position lives in its own VBO, separate from the
// interleaved normal/joint/weight buffer, so a viseme change is one
// glBufferSubData of the position region. This is 4738 vertices of
// work per viseme change, at viseme rate, which is negligible. The
// GPU path for a per-frame morph would need an attribute-based vertex
// index that GLSL 150 does not have.
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
    // Null for non-face primitives, whose positions are in the
    // interleaved buffer alongside the rest.
    GLuint posVbo = 0;

    GLuint ibo = 0;
    GLsizei indexCount = 0;
    int materialIndex = -1;
    int vertexCount = 0;
    int morphTargetCount = 0;

    // Set only on the face mesh. When true, the renderer can
    // recompute the displaced positions on the CPU when the morph
    // weights change and rewrite posVbo.
    bool isFacePrimitive = false;

    // Source data for the CPU morph blend. Present only on the face
    // primitive. basePositions is 3 floats per vertex. morphDeltas is
    // targetCount blocks of 3 floats per vertex, in target-major
    // order.
    QVector<float> basePositions;
    QVector<float> morphDeltas;
  };

  struct GpuMesh {
    QString name;
    QVector<GpuPrimitive> primitives;
  };

  bool compileShaders();
  bool uploadMesh(const AvatarMesh &mesh, GpuMesh &out,
                  bool captureMorphSource);
  void freeGpuData();
  void uploadSkinBuffer();

  // Recompute the face position VBO from basePositions, morphDeltas,
  // and m_morphWeights. One glBufferSubData per face primitive.
  void rebuildFacePositions();

  GLuint m_programId = 0;
  GLint m_uViewProj = -1;
  GLint m_uColor = -1;
  GLint m_uSkinBuffer = -1;

  GLuint m_skinTbo = 0;
  GLuint m_skinTexture = 0;
  int m_skinCapacity = 0;

  QVector<GpuMesh> m_meshes;

  std::vector<QMatrix4x4> m_skinningMatrices;
  std::vector<float> m_morphWeights;

  bool m_ready = false;
};