#pragma once

#include "AvatarMeshData.h"

#include <QMatrix4x4>
#include <QObject>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions_3_2_Core>
#include <QVector>

#include <vector>

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
    GLuint vbo = 0;
    GLuint posVbo = 0;
    GLuint ibo = 0;
    GLsizei indexCount = 0;
    int materialIndex = -1;
    int vertexCount = 0;
    int morphTargetCount = 0;
    int baseColorTextureIndex = -1;
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
  bool uploadTextures(const AvatarMeshData &meshData);

  GLuint m_programId = 0;
  GLint m_uViewProj = -1;
  GLint m_uColor = -1;
  GLint m_uSkinBuffer = -1;

  GLuint m_skinTbo = 0;
  GLuint m_skinTexture = 0;
  int m_skinCapacity = 0;
  GLint m_uBaseColor = -1;
  QVector<GLuint> m_textures;

  GLuint m_whiteTexture = 0;

  QVector<GpuMesh> m_meshes;

  std::vector<QMatrix4x4> m_skinningMatrices;
  std::vector<float> m_morphWeights;

  bool m_ready = false;
};