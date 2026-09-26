#pragma once

#include "AvatarMeshData.h"

#include <QString>

// Loads a GLB file into an AvatarMeshData struct.
//
// The loader reads the file from a Qt resource path ("qrc:/...") or a
// filesystem path. The GLB binary chunk is passed to tinygltf in
// memory; nothing touches the filesystem after the initial read.
//
// Nothing in the loaded data references tinygltf. The model is
// flattened at load time so the renderer has no glTF dependency.
class AvatarMeshLoader {
public:
  // Load the GLB and fill out. Returns false on any failure; the
  // reason is written to the qWarning log and optionally to error.
  static bool load(const QString &path, AvatarMeshData &out,
                   QString *error = nullptr);
};