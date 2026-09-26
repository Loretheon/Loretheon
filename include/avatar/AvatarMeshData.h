#pragma once

#include <QByteArray>
#include <QHash>
#include <QMatrix4x4>
#include <QString>
#include <QStringList>
#include <QVector>
#include <cstdint>

struct AvatarPrimitive {
  QVector<float> positions;
  QVector<float> normals;
  QVector<float> uvs;
  QVector<quint16> jointIndices;
  QVector<float> jointWeights;
  QVector<quint32> indices;

  QVector<QVector<float>> morphTargets;

  int materialIndex = -1;

  QStringList morphTargetNames;

  int vertexCount() const { return positions.size() / 3; }
  int morphTargetCount() const { return morphTargets.size(); }
};

struct AvatarMesh {
  QString name;
  QVector<AvatarPrimitive> primitives;
};

struct AvatarSkin {
  QString name;
  QStringList jointNames;
  QVector<float> inverseBindPoses;

  int jointCount() const { return jointNames.size(); }

  bool isConsistent() const {
    return inverseBindPoses.size() == 16 * jointNames.size();
  }
};

// One decoded texture, kept as its encoded bytes. The loader does not
// decode. The renderer builds a QImage from bytes at upload time.
struct AvatarTexture {
  QString name;
  QString mimeType;
  QByteArray bytes;
};

// One glTF material, flattened. baseColorTextureIndex is an index
// into AvatarMeshData::textures, or -1.
struct AvatarMaterial {
  QString name;
  int baseColorTextureIndex = -1;
  QVector4D baseColorFactor = QVector4D(1.0f, 1.0f, 1.0f, 1.0f);
};

struct AvatarMeshData {
  QVector<AvatarMesh> meshes;
  QVector<AvatarSkin> skins;
  QVector<AvatarTexture> textures;
  QVector<AvatarMaterial> materials;

  QStringList materialNames;

  int faceMeshIndex = -1;

  QHash<QString, int> faceMorphNameToIndex;

  int totalPrimitives() const {
    int n = 0;
    for (const AvatarMesh &m : meshes) {
      n += m.primitives.size();
    }
    return n;
  }

  int totalMorphTargets() const {
    int n = 0;
    for (const AvatarMesh &m : meshes) {
      for (const AvatarPrimitive &p : m.primitives) {
        n += p.morphTargetCount();
      }
    }
    return n;
  }
};