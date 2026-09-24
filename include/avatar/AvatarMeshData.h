#pragma once

#include <QHash>
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

struct AvatarMeshData {
  QVector<AvatarMesh> meshes;
  QVector<AvatarSkin> skins;

  QStringList materialNames;

  int faceMeshIndex = -1;

  // Morph target name to index on the face mesh. Built by the loader
  // from the first primitive that carries named targets. All six
  // primitives of CC_Base_Body share the same 148 targets in the same
  // order, so one map is enough.
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