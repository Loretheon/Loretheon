#include "../../include/avatar/AvatarRenderer.h"

#include <QDebug>
#include <QImage>
#include <QOpenGLShader>

namespace {

constexpr int kSkinCapacity = 256;
constexpr int kFloatsPerMatrix = 16;
constexpr int kSkinBufferFloats = kSkinCapacity * kFloatsPerMatrix;

constexpr int kInterleavedStride = 44;

constexpr auto kVertexShader = R"(
#version 150 core

in vec3 aPosition;
in vec3 aNormal;
in uvec4 aJointIndices;
in vec4 aJointWeights;
in vec2 aTexCoord;

uniform mat4 uViewProj;
uniform samplerBuffer uSkinBuffer;

out vec3 vNormal;
out vec2 vTexCoord;

mat4 fetchSkin(int idx) {
    int base = idx * 4;
    return mat4(
        texelFetch(uSkinBuffer, base + 0),
        texelFetch(uSkinBuffer, base + 1),
        texelFetch(uSkinBuffer, base + 2),
        texelFetch(uSkinBuffer, base + 3));
}

void main() {
    int i0 = clamp(int(aJointIndices.x), 0, 255);
    int i1 = clamp(int(aJointIndices.y), 0, 255);
    int i2 = clamp(int(aJointIndices.z), 0, 255);
    int i3 = clamp(int(aJointIndices.w), 0, 255);

    mat4 skin =
        aJointWeights.x * fetchSkin(i0) +
        aJointWeights.y * fetchSkin(i1) +
        aJointWeights.z * fetchSkin(i2) +
        aJointWeights.w * fetchSkin(i3);

    vec4 skinnedPosition = skin * vec4(aPosition, 1.0);
    gl_Position = uViewProj * skinnedPosition;

    vNormal = normalize(mat3(skin) * aNormal);
    vTexCoord = aTexCoord;
}
)";

constexpr auto kFragmentShader = R"(
#version 150 core

in vec3 vNormal;
in vec2 vTexCoord;
out vec4 fragColor;

uniform vec4 uColor;
uniform sampler2D uBaseColor;

void main() {
    vec3 lightDir = normalize(vec3(0.3, 0.7, 0.6));
    float lambert = max(dot(normalize(vNormal), lightDir), 0.0);
    float ambient = 0.35;
    float shade = ambient + lambert * 0.65;

    vec4 texel = texture(uBaseColor, vTexCoord);

    fragColor = vec4(texel.rgb * uColor.rgb * shade,
                     texel.a * uColor.a);
}
)";

QVector4D colorForMesh(const QString &name) {
  if (name == QStringLiteral("CC_Base_Body")) {
    return QVector4D(0.85f, 0.65f, 0.55f, 1.0f);
  }
  if (name == QStringLiteral("CC_Base_Eye")) {
    return QVector4D(0.35f, 0.45f, 0.55f, 1.0f);
  }
  if (name == QStringLiteral("CC_Base_EyeOcclusion")) {
    return QVector4D(0.10f, 0.10f, 0.10f, 1.0f);
  }
  if (name == QStringLiteral("CC_Base_TearLine")) {
    return QVector4D(0.80f, 0.85f, 0.90f, 1.0f);
  }
  if (name == QStringLiteral("CC_Base_Tongue")) {
    return QVector4D(0.85f, 0.40f, 0.40f, 1.0f);
  }
  if (name == QStringLiteral("CC_Toon_Teeth_01")) {
    return QVector4D(0.95f, 0.95f, 0.95f, 1.0f);
  }
  if (name == QStringLiteral("Toon_Eyebrows")) {
    return QVector4D(0.15f, 0.10f, 0.08f, 1.0f);
  }

  return QVector4D(0.5f, 0.5f, 0.5f, 1.0f);
}

} // namespace

AvatarRenderer::AvatarRenderer(QObject *parent) : QObject(parent) {}

AvatarRenderer::~AvatarRenderer() {}

bool AvatarRenderer::initialize(const AvatarMeshData &meshData) {
  if (m_ready) {
    return true;
  }

  initializeOpenGLFunctions();

  if (!compileShaders()) {
    return false;
  }

  m_uViewProj = glGetUniformLocation(m_programId, "uViewProj");
  m_uSkinBuffer = glGetUniformLocation(m_programId, "uSkinBuffer");
  m_uColor = glGetUniformLocation(m_programId, "uColor");
  m_uBaseColor = glGetUniformLocation(m_programId, "uBaseColor");

  if (m_uViewProj < 0 || m_uSkinBuffer < 0 || m_uColor < 0 ||
      m_uBaseColor < 0) {
    qWarning() << "[AvatarRenderer] Missing uniforms."
               << "uViewProj:" << m_uViewProj
               << "uSkinBuffer:" << m_uSkinBuffer
               << "uColor:" << m_uColor
               << "uBaseColor:" << m_uBaseColor;
    return false;
  }

  m_skinCapacity = kSkinCapacity;

  glGenBuffers(1, &m_skinTbo);
  glBindBuffer(GL_TEXTURE_BUFFER, m_skinTbo);
  glBufferData(GL_TEXTURE_BUFFER, kSkinBufferFloats * sizeof(GLfloat),
               nullptr, GL_DYNAMIC_DRAW);
  glBindBuffer(GL_TEXTURE_BUFFER, 0);

  glGenTextures(1, &m_skinTexture);
  glBindTexture(GL_TEXTURE_BUFFER, m_skinTexture);
  glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA32F, m_skinTbo);
  glBindTexture(GL_TEXTURE_BUFFER, 0);

  m_meshes.clear();
  m_meshes.reserve(meshData.meshes.size());

  for (const AvatarMesh &mesh : meshData.meshes) {
    GpuMesh gpuMesh;
    gpuMesh.name = mesh.name;

    const bool isFace = (mesh.name == QStringLiteral("CC_Base_Body"));

    if (!uploadMesh(mesh, gpuMesh, isFace, meshData)) {
      qWarning() << "[AvatarRenderer] Failed to upload mesh" << mesh.name;
      freeGpuData();
      return false;
    }

    m_meshes.append(gpuMesh);
  }

  if (!meshData.skins.isEmpty()) {
    m_skinningMatrices.resize(
        static_cast<size_t>(meshData.skins.first().jointCount()));
    for (auto &m : m_skinningMatrices) {
      m.setToIdentity();
    }
  }

  uploadSkinBuffer();

  if (!uploadTextures(meshData)) {
    qWarning() << "[AvatarRenderer] Texture upload failed.";
  }

  m_ready = true;

  qDebug() << "[AvatarRenderer] Initialized:"
           << m_meshes.size() << "meshes uploaded, skin TBO"
           << m_skinTbo << "capacity" << m_skinCapacity << "matrices,"
           << m_textures.size() << "textures.";

  return true;
}

bool AvatarRenderer::uploadTextures(const AvatarMeshData &meshData) {
  m_textures.clear();
  m_textures.resize(meshData.textures.size());

  for (int i = 0; i < meshData.textures.size(); ++i) {
    const AvatarTexture &tex = meshData.textures.at(i);

    m_textures[i] = 0;

    if (tex.bytes.isEmpty()) {
      qWarning() << "[AvatarRenderer] Texture" << i
                 << tex.name << "has no bytes; skipping.";
      continue;
    }

    QImage image = QImage::fromData(tex.bytes);

    if (image.isNull()) {
      qWarning() << "[AvatarRenderer] Texture" << i
                 << tex.name << "failed to decode.";
      continue;
    }

    if (image.format() != QImage::Format_RGBA8888) {
      image = image.convertToFormat(QImage::Format_RGBA8888);
    }

    GLuint id = 0;
    glGenTextures(1, &id);
    glBindTexture(GL_TEXTURE_2D, id);

    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA,
                 image.width(), image.height(), 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, image.constBits());

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                    GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    m_textures[i] = id;

    qDebug() << "[AvatarRenderer] Texture" << i
             << tex.name
             << image.width() << "x" << image.height()
             << "gl id" << id;
  }

  {
    const unsigned char white[4] = {255, 255, 255, 255};

    glGenTextures(1, &m_whiteTexture);
    glBindTexture(GL_TEXTURE_2D, m_whiteTexture);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, white);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D, 0);
  }

  qDebug() << "[AvatarRenderer] Textures uploaded:"
           << m_textures.size() << "white fallback"
           << m_whiteTexture;

  return true;
}

bool AvatarRenderer::compileShaders() {
  const GLuint vertexId = glCreateShader(GL_VERTEX_SHADER);
  const GLuint fragmentId = glCreateShader(GL_FRAGMENT_SHADER);

  auto compile = [this](GLuint shader, const char *source,
                        const char *label) -> bool {
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    GLint compiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);

    if (!compiled) {
      GLint logLength = 0;
      glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);
      QByteArray log(logLength, 0);
      glGetShaderInfoLog(shader, logLength, nullptr, log.data());

      qWarning() << "[AvatarRenderer]" << label << "compile failed:\n"
                 << log;
      return false;
    }

    return true;
  };

  if (!compile(vertexId, kVertexShader, "Vertex")) {
    glDeleteShader(vertexId);
    glDeleteShader(fragmentId);
    return false;
  }

  if (!compile(fragmentId, kFragmentShader, "Fragment")) {
    glDeleteShader(vertexId);
    glDeleteShader(fragmentId);
    return false;
  }

  m_programId = glCreateProgram();
  glAttachShader(m_programId, vertexId);
  glAttachShader(m_programId, fragmentId);

  glBindAttribLocation(m_programId, 0, "aPosition");
  glBindAttribLocation(m_programId, 1, "aNormal");
  glBindAttribLocation(m_programId, 2, "aJointIndices");
  glBindAttribLocation(m_programId, 3, "aJointWeights");
  glBindAttribLocation(m_programId, 4, "aTexCoord");

  glLinkProgram(m_programId);

  glDeleteShader(vertexId);
  glDeleteShader(fragmentId);

  GLint linked = 0;
  glGetProgramiv(m_programId, GL_LINK_STATUS, &linked);

  if (!linked) {
    GLint logLength = 0;
    glGetProgramiv(m_programId, GL_INFO_LOG_LENGTH, &logLength);
    QByteArray log(logLength, 0);
    glGetProgramInfoLog(m_programId, logLength, nullptr, log.data());

    qWarning() << "[AvatarRenderer] Program link failed:\n" << log;

    glDeleteProgram(m_programId);
    m_programId = 0;
    return false;
  }

  return true;
}

bool AvatarRenderer::uploadMesh(const AvatarMesh &mesh, GpuMesh &out,
                                bool captureMorphSource,
                                const AvatarMeshData &meshData) {
  for (const AvatarPrimitive &prim : mesh.primitives) {
    const int vertexCount = prim.vertexCount();

    if (vertexCount <= 0 || prim.indices.isEmpty()) {
      continue;
    }

    GpuPrimitive gpuPrim;
    gpuPrim.indexCount = static_cast<GLsizei>(prim.indices.size());
    gpuPrim.materialIndex = prim.materialIndex;
    gpuPrim.vertexCount = vertexCount;
    gpuPrim.morphTargetCount = prim.morphTargetCount();
    gpuPrim.isFacePrimitive = captureMorphSource;

    gpuPrim.baseColorTextureIndex = -1;

    if (prim.materialIndex >= 0 &&
        prim.materialIndex < meshData.materials.size()) {
      gpuPrim.baseColorTextureIndex =
          meshData.materials.at(prim.materialIndex).baseColorTextureIndex;
    }

    QByteArray interleaved;
    interleaved.resize(vertexCount * kInterleavedStride);

    auto *ip = reinterpret_cast<unsigned char *>(interleaved.data());

    for (int i = 0; i < vertexCount; ++i) {
      unsigned char *base = ip + i * kInterleavedStride;

      const int posBase = i * 3;
      const int jointBase = i * 4;
      const int uvBase = i * 2;

      if (prim.normals.size() >= posBase + 3) {
        std::memcpy(base + 0, &prim.normals[posBase], 12);
      }

      if (prim.jointIndices.size() >= jointBase + 4) {
        std::memcpy(base + 12, &prim.jointIndices[jointBase], 8);
      }

      if (prim.jointWeights.size() >= jointBase + 4) {
        std::memcpy(base + 20, &prim.jointWeights[jointBase], 16);
      }

      if (prim.uvs.size() >= uvBase + 2) {
        std::memcpy(base + 36, &prim.uvs[uvBase], 8);
      }
    }

    QByteArray positions;
    positions.resize(vertexCount * 3 * static_cast<int>(sizeof(float)));

    if (prim.positions.size() >= vertexCount * 3) {
      std::memcpy(positions.data(), prim.positions.constData(),
                  static_cast<size_t>(vertexCount) * 3 * sizeof(float));
    }

    if (captureMorphSource) {
      gpuPrim.basePositions = prim.positions;

      gpuPrim.morphDeltas.clear();

      const int targetCount = prim.morphTargetCount();

      if (targetCount > 0) {
        gpuPrim.morphDeltas.reserve(targetCount * vertexCount * 3);

        for (int t = 0; t < targetCount; ++t) {
          const QVector<float> &deltas = prim.morphTargets.at(t);

          if (deltas.size() == vertexCount * 3) {
            gpuPrim.morphDeltas.append(deltas);
          } else {
            QVector<float> padded(vertexCount * 3, 0.0f);

            const int n = qMin(deltas.size(), vertexCount * 3);

            for (int k = 0; k < n; ++k) {
              padded[k] = deltas[k];
            }

            gpuPrim.morphDeltas.append(padded);
          }
        }
      }
    }

    glGenVertexArrays(1, &gpuPrim.vao);
    glBindVertexArray(gpuPrim.vao);

    glGenBuffers(1, &gpuPrim.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, gpuPrim.vbo);
    glBufferData(GL_ARRAY_BUFFER, interleaved.size(),
                 interleaved.constData(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, kInterleavedStride,
                          reinterpret_cast<void *>(0));

    glEnableVertexAttribArray(2);
    glVertexAttribIPointer(2, 4, GL_UNSIGNED_SHORT, kInterleavedStride,
                           reinterpret_cast<void *>(12));

    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 4, GL_FLOAT, GL_FALSE, kInterleavedStride,
                          reinterpret_cast<void *>(20));

    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 2, GL_FLOAT, GL_FALSE, kInterleavedStride,
                          reinterpret_cast<void *>(36));

    glGenBuffers(1, &gpuPrim.posVbo);
    glBindBuffer(GL_ARRAY_BUFFER, gpuPrim.posVbo);
    glBufferData(GL_ARRAY_BUFFER, positions.size(), positions.constData(),
                 captureMorphSource ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0,
                          reinterpret_cast<void *>(0));

    glGenBuffers(1, &gpuPrim.ibo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, gpuPrim.ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(prim.indices.size() *
                                         sizeof(quint32)),
                 prim.indices.constData(), GL_STATIC_DRAW);

    glBindVertexArray(0);

    out.primitives.append(gpuPrim);
  }

  return true;
}

void AvatarRenderer::uploadSkinBuffer() {
  if (m_skinTbo == 0) {
    return;
  }

  std::vector<float> data(static_cast<size_t>(kSkinBufferFloats), 0.0f);

  const int count = qMin(static_cast<int>(m_skinningMatrices.size()),
                         m_skinCapacity);

  for (int i = 0; i < count; ++i) {
    const QMatrix4x4 &m = m_skinningMatrices[static_cast<size_t>(i)];
    std::memcpy(&data[static_cast<size_t>(i) * 16], m.constData(),
                16 * sizeof(float));
  }

  glBindBuffer(GL_TEXTURE_BUFFER, m_skinTbo);
  glBufferSubData(GL_TEXTURE_BUFFER, 0,
                  data.size() * sizeof(GLfloat), data.data());
  glBindBuffer(GL_TEXTURE_BUFFER, 0);
}

void AvatarRenderer::rebuildFacePositions() {
  if (m_morphWeights.empty()) {
    return;
  }

  for (GpuMesh &mesh : m_meshes) {
    for (GpuPrimitive &prim : mesh.primitives) {
      if (!prim.isFacePrimitive) {
        continue;
      }

      if (prim.basePositions.isEmpty() || prim.posVbo == 0) {
        continue;
      }

      const int vertexCount = prim.vertexCount;
      const int floats = vertexCount * 3;

      std::vector<float> displaced(
          prim.basePositions.constData(),
          prim.basePositions.constData() + prim.basePositions.size());

      if (displaced.size() < static_cast<size_t>(floats)) {
        displaced.resize(static_cast<size_t>(floats), 0.0f);
      }

      const int targetCount = prim.morphTargetCount;

      for (int t = 0; t < targetCount; ++t) {
        if (t >= static_cast<int>(m_morphWeights.size())) {
          break;
        }

        const float w = m_morphWeights[static_cast<size_t>(t)];

        if (w == 0.0f) {
          continue;
        }

        const float *delta =
            prim.morphDeltas.constData() +
            static_cast<qsizetype>(t) * floats;

        for (int k = 0; k < floats; ++k) {
          displaced[static_cast<size_t>(k)] += w * delta[k];
        }
      }

      glBindBuffer(GL_ARRAY_BUFFER, prim.posVbo);
      glBufferSubData(GL_ARRAY_BUFFER, 0,
                      static_cast<GLsizeiptr>(floats * sizeof(float)),
                      displaced.data());
      glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
  }
}

void AvatarRenderer::setSkinningMatrices(
    const std::vector<QMatrix4x4> &matrices) {
  m_skinningMatrices = matrices;
  uploadSkinBuffer();
}

void AvatarRenderer::setMorphWeights(const std::vector<float> &weights) {
  m_morphWeights = weights;
  rebuildFacePositions();
}

void AvatarRenderer::render(const QMatrix4x4 &viewProjection) {
  if (!m_ready) {
    return;
  }

  glUseProgram(m_programId);

  glEnable(GL_DEPTH_TEST);
  glDepthFunc(GL_LESS);
  glDepthMask(GL_TRUE);
  glClear(GL_DEPTH_BUFFER_BIT);

  glDisable(GL_CULL_FACE);

  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

  glUniformMatrix4fv(m_uViewProj, 1, GL_FALSE,
                     viewProjection.constData());

  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_BUFFER, m_skinTexture);
  glUniform1i(m_uSkinBuffer, 0);

  glActiveTexture(GL_TEXTURE1);
  glUniform1i(m_uBaseColor, 1);

  int primitiveCounter = 0;
  int texturedCounter = 0;

  for (const GpuMesh &mesh : m_meshes) {
    const QVector4D fallback = colorForMesh(mesh.name);

    for (const GpuPrimitive &prim : mesh.primitives) {
      QVector4D color = fallback;
      GLuint texture = m_whiteTexture;

      if (prim.baseColorTextureIndex >= 0 &&
          prim.baseColorTextureIndex < m_textures.size() &&
          m_textures.at(prim.baseColorTextureIndex) != 0) {
        texture = m_textures.at(prim.baseColorTextureIndex);
        color = QVector4D(1.0f, 1.0f, 1.0f, 1.0f);
        ++texturedCounter;
      }

      glActiveTexture(GL_TEXTURE1);
      glBindTexture(GL_TEXTURE_2D, texture);

      glUniform4fv(m_uColor, 1, reinterpret_cast<const GLfloat *>(&color));

      glBindVertexArray(prim.vao);
      glDrawElements(GL_TRIANGLES, prim.indexCount, GL_UNSIGNED_INT,
                     nullptr);
      glBindVertexArray(0);

      ++primitiveCounter;
    }
  }

  glActiveTexture(GL_TEXTURE1);
  glBindTexture(GL_TEXTURE_2D, 0);
  glActiveTexture(GL_TEXTURE0);
  glBindTexture(GL_TEXTURE_BUFFER, 0);
  glUseProgram(0);

  static int counter = 0;
  if (++counter == 60) {
    const GLenum err = glGetError();
    qDebug() << "[AvatarRenderer] frame 60:"
             << "meshes:" << m_meshes.size()
             << "skinning:" << m_skinningMatrices.size()
             << "morph weights:" << m_morphWeights.size()
             << "textures:" << m_textures.size()
             << "primitives drawn:" << primitiveCounter
             << "textured:" << texturedCounter
             << "glError:" << err;
  }
}

void AvatarRenderer::destroy() {
  if (!m_ready) {
    return;
  }

  freeGpuData();

  for (GLuint id : m_textures) {
    if (id != 0) {
      glDeleteTextures(1, &id);
    }
  }
  m_textures.clear();

  if (m_whiteTexture != 0) {
    glDeleteTextures(1, &m_whiteTexture);
    m_whiteTexture = 0;
  }

  if (m_skinTexture != 0) {
    glDeleteTextures(1, &m_skinTexture);
    m_skinTexture = 0;
  }

  if (m_skinTbo != 0) {
    glDeleteBuffers(1, &m_skinTbo);
    m_skinTbo = 0;
  }

  if (m_programId != 0) {
    glDeleteProgram(m_programId);
    m_programId = 0;
  }

  m_ready = false;

  qDebug() << "[AvatarRenderer] Destroyed.";
}

void AvatarRenderer::freeGpuData() {
  for (GpuMesh &mesh : m_meshes) {
    for (GpuPrimitive &prim : mesh.primitives) {
      if (prim.vao != 0) {
        glDeleteVertexArrays(1, &prim.vao);
      }
      if (prim.vbo != 0) {
        glDeleteBuffers(1, &prim.vbo);
      }
      if (prim.posVbo != 0) {
        glDeleteBuffers(1, &prim.posVbo);
      }
      if (prim.ibo != 0) {
        glDeleteBuffers(1, &prim.ibo);
      }
    }
  }

  m_meshes.clear();
}