#include "../../include/avatar/AvatarMeshLoader.h"

#include <QDebug>
#include <QFile>
#include <QFileInfo>


#define TINYGLTF_IMPLEMENTATION
#define TINYGLTF_NO_STB_IMAGE
#define TINYGLTF_NO_STB_IMAGE_WRITE
#define TINYGLTF_NO_EXTERNAL_IMAGE
#include <tiny_gltf.h>
namespace {

bool readFloatAccessor(const tinygltf::Model &model, int accessorIndex,
                       int componentsPerElement, QVector<float> &out) {
  if (accessorIndex < 0 ||
      accessorIndex >= static_cast<int>(model.accessors.size())) {
    return false;
  }

  const tinygltf::Accessor &accessor = model.accessors[accessorIndex];

  const int componentSize = tinygltf::GetComponentSizeInBytes(
      static_cast<uint32_t>(accessor.componentType));

  if (componentSize == 0) {
    return false;
  }

  const int elementSize =
      componentSize * tinygltf::GetNumComponentsInType(
                         static_cast<uint32_t>(accessor.type));

  const size_t count = static_cast<size_t>(accessor.count);

  out.clear();
  out.resize(static_cast<int>(count * componentsPerElement));
  out.fill(0.0f);

  auto readElement = [&](const unsigned char *ptr, size_t elementIndex) {
    for (int c = 0; c < componentsPerElement; ++c) {
      float value = 0.0f;

      switch (accessor.componentType) {
      case TINYGLTF_COMPONENT_TYPE_FLOAT: {
        float v;
        std::memcpy(&v, ptr + c * componentSize, sizeof(float));
        value = v;
        break;
      }
      case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
        uint16_t v;
        std::memcpy(&v, ptr + c * componentSize, sizeof(uint16_t));
        value = static_cast<float>(v);
        break;
      }
      case TINYGLTF_COMPONENT_TYPE_SHORT: {
        int16_t v;
        std::memcpy(&v, ptr + c * componentSize, sizeof(int16_t));
        value = static_cast<float>(v);
        break;
      }
      case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE: {
        uint8_t v = *(ptr + c * componentSize);
        value = static_cast<float>(v);
        break;
      }
      case TINYGLTF_COMPONENT_TYPE_BYTE: {
        int8_t v;
        std::memcpy(&v, ptr + c * componentSize, sizeof(int8_t));
        value = static_cast<float>(v);
        break;
      }
      case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT: {
        uint32_t v;
        std::memcpy(&v, ptr + c * componentSize, sizeof(uint32_t));
        value = static_cast<float>(v);
        break;
      }
      default:
        break;
      }

      out[static_cast<int>(elementIndex * componentsPerElement) + c] = value;
    }
  };

  if (accessor.bufferView >= 0 &&
      accessor.bufferView < static_cast<int>(model.bufferViews.size())) {
    const tinygltf::BufferView &view =
        model.bufferViews[accessor.bufferView];

    if (view.buffer >= 0 &&
        view.buffer < static_cast<int>(model.buffers.size())) {
      const tinygltf::Buffer &buffer = model.buffers[view.buffer];

      int stride = view.byteStride;

      if (stride == 0) {
        stride = elementSize;
      }

      const size_t base =
          static_cast<size_t>(view.byteOffset) + accessor.byteOffset;

      const bool baseIsEmpty =
          (view.byteLength == 0) || (base >= buffer.data.size());

      if (!baseIsEmpty) {
        for (size_t i = 0; i < count; ++i) {
          const size_t offset = base + i * static_cast<size_t>(stride);

          if (offset + elementSize > buffer.data.size()) {
            qWarning() << "[AvatarMeshLoader] Accessor read out of bounds."
                       << "accessor:" << accessorIndex
                       << "offset:" << offset
                       << "buffer size:" << buffer.data.size();
            return false;
          }

          readElement(buffer.data.data() + offset, i);
        }
      }
    }
  }

  if (accessor.sparse.isSparse) {
    const tinygltf::Accessor::Sparse &sparse = accessor.sparse;

    if (sparse.indices.bufferView < 0 ||
        sparse.indices.bufferView >=
            static_cast<int>(model.bufferViews.size()) ||
        sparse.values.bufferView < 0 ||
        sparse.values.bufferView >=
            static_cast<int>(model.bufferViews.size())) {
      return false;
    }

    const tinygltf::BufferView &indexView =
        model.bufferViews[sparse.indices.bufferView];
    const tinygltf::BufferView &valueView =
        model.bufferViews[sparse.values.bufferView];

    if (indexView.buffer < 0 ||
        indexView.buffer >= static_cast<int>(model.buffers.size()) ||
        valueView.buffer < 0 ||
        valueView.buffer >= static_cast<int>(model.buffers.size())) {
      return false;
    }

    const tinygltf::Buffer &indexBuffer = model.buffers[indexView.buffer];
    const tinygltf::Buffer &valueBuffer = model.buffers[valueView.buffer];

    const int indexComponentSize = tinygltf::GetComponentSizeInBytes(
        static_cast<uint32_t>(sparse.indices.componentType));

    if (indexComponentSize == 0) {
      return false;
    }

    const size_t indexBase =
        static_cast<size_t>(indexView.byteOffset) +
        sparse.indices.byteOffset;
    const size_t valueBase =
        static_cast<size_t>(valueView.byteOffset) +
        sparse.values.byteOffset;

    const size_t sparseCount = static_cast<size_t>(sparse.count);

    for (size_t s = 0; s < sparseCount; ++s) {
      const size_t indexOffset =
          indexBase + s * static_cast<size_t>(indexComponentSize);

      if (indexOffset + static_cast<size_t>(indexComponentSize) >
          indexBuffer.data.size()) {
        return false;
      }

      const unsigned char *indexPtr =
          indexBuffer.data.data() + indexOffset;

      uint32_t targetIndex = 0;

      switch (sparse.indices.componentType) {
      case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
        std::memcpy(&targetIndex, indexPtr, sizeof(uint32_t));
        break;
      case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
        uint16_t v;
        std::memcpy(&v, indexPtr, sizeof(uint16_t));
        targetIndex = v;
        break;
      }
      case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
        targetIndex = static_cast<uint32_t>(*indexPtr);
        break;
      default:
        return false;
      }

      if (targetIndex >= count) {
        return false;
      }

      const size_t valueOffset =
          valueBase + s * static_cast<size_t>(elementSize);

      if (valueOffset + static_cast<size_t>(elementSize) >
          valueBuffer.data.size()) {
        return false;
      }

      readElement(valueBuffer.data.data() + valueOffset,
                  static_cast<size_t>(targetIndex));
    }
  }

  return true;
}

bool readU16Accessor(const tinygltf::Model &model, int accessorIndex,
                     int componentsPerElement, QVector<quint16> &out) {
  if (accessorIndex < 0 ||
      accessorIndex >= static_cast<int>(model.accessors.size())) {
    return false;
  }

  const tinygltf::Accessor &accessor = model.accessors[accessorIndex];

  if (accessor.bufferView < 0 ||
      accessor.bufferView >= static_cast<int>(model.bufferViews.size())) {
    return false;
  }

  const tinygltf::BufferView &view =
      model.bufferViews[accessor.bufferView];

  if (view.buffer < 0 ||
      view.buffer >= static_cast<int>(model.buffers.size())) {
    return false;
  }

  const tinygltf::Buffer &buffer = model.buffers[view.buffer];

  const int componentSize = tinygltf::GetComponentSizeInBytes(
      static_cast<uint32_t>(accessor.componentType));

  if (componentSize == 0) {
    return false;
  }

  const int elementSize =
      componentSize * tinygltf::GetNumComponentsInType(
                         static_cast<uint32_t>(accessor.type));

  int stride = view.byteStride;

  if (stride == 0) {
    stride = elementSize;
  }

  const size_t base =
      static_cast<size_t>(view.byteOffset) + accessor.byteOffset;

  const size_t count = static_cast<size_t>(accessor.count);

  out.clear();
  out.reserve(static_cast<int>(count * componentsPerElement));

  for (size_t i = 0; i < count; ++i) {
    const size_t offset = base + i * static_cast<size_t>(stride);

    if (offset + elementSize > buffer.data.size()) {
      qWarning() << "[AvatarMeshLoader] Joint accessor out of bounds.";
      return false;
    }

    const unsigned char *ptr = buffer.data.data() + offset;

    for (int c = 0; c < componentsPerElement; ++c) {
      uint16_t value = 0;

      switch (accessor.componentType) {
      case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
        std::memcpy(&value, ptr + c * componentSize, sizeof(uint16_t));
        break;
      case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
        value = static_cast<uint16_t>(*(ptr + c * componentSize));
        break;
      default:
        return false;
      }

      out.append(value);
    }
  }

  return true;
}

bool readIndexAccessor(const tinygltf::Model &model, int accessorIndex,
                       QVector<quint32> &out) {
  if (accessorIndex < 0 ||
      accessorIndex >= static_cast<int>(model.accessors.size())) {
    return false;
  }

  const tinygltf::Accessor &accessor = model.accessors[accessorIndex];

  if (accessor.bufferView < 0 ||
      accessor.bufferView >= static_cast<int>(model.bufferViews.size())) {
    return false;
  }

  const tinygltf::BufferView &view =
      model.bufferViews[accessor.bufferView];

  if (view.buffer < 0 ||
      view.buffer >= static_cast<int>(model.buffers.size())) {
    return false;
  }

  const tinygltf::Buffer &buffer = model.buffers[view.buffer];

  const int componentSize = tinygltf::GetComponentSizeInBytes(
      static_cast<uint32_t>(accessor.componentType));

  if (componentSize == 0) {
    return false;
  }

  int stride = view.byteStride;

  if (stride == 0) {
    stride = componentSize;
  }

  const size_t base =
      static_cast<size_t>(view.byteOffset) + accessor.byteOffset;

  const size_t count = static_cast<size_t>(accessor.count);

  out.clear();
  out.reserve(static_cast<int>(count));

  for (size_t i = 0; i < count; ++i) {
    const size_t offset = base + i * static_cast<size_t>(stride);

    if (offset + componentSize > buffer.data.size()) {
      qWarning() << "[AvatarMeshLoader] Index accessor out of bounds.";
      return false;
    }

    const unsigned char *ptr = buffer.data.data() + offset;

    uint32_t value = 0;

    switch (accessor.componentType) {
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_INT:
      std::memcpy(&value, ptr, sizeof(uint32_t));
      break;
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_SHORT: {
      uint16_t v;
      std::memcpy(&v, ptr, sizeof(uint16_t));
      value = v;
      break;
    }
    case TINYGLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
      value = static_cast<uint32_t>(*ptr);
      break;
    default:
      return false;
    }

    out.append(value);
  }

  return true;
}

QString morphTargetName(const tinygltf::Mesh &mesh,
                        const tinygltf::Primitive &primitive,
                        int targetIndex) {
  auto nameFromValue = [targetIndex](const tinygltf::Value &names)
      -> QString {
    if (!names.IsArray()) {
      return {};
    }

    if (targetIndex >= static_cast<int>(names.ArrayLen())) {
      return {};
    }

    const tinygltf::Value &entry = names.Get(targetIndex);

    if (!entry.IsString()) {
      return {};
    }

    return QString::fromStdString(entry.Get<std::string>());
  };

  if (primitive.extras.Has("targetNames")) {
    const QString name =
        nameFromValue(primitive.extras.Get("targetNames"));

    if (!name.isEmpty()) {
      return name;
    }
  }

  if (mesh.extras.Has("targetNames")) {
    const QString name = nameFromValue(mesh.extras.Get("targetNames"));

    if (!name.isEmpty()) {
      return name;
    }
  }

  return {};
}


bool loadImageDataNoOp(tinygltf::Image *image, const int /*imageIndex*/,
                       std::string * /*err*/, std::string * /*warn*/,
                       int /*reqWidth*/, int /*reqHeight*/,
                       const unsigned char * /*bytes*/,
                       int /*size*/, void * /*userData*/) {
  if (image) {
    image->width = 0;
    image->height = 0;
    image->component = 0;
    image->bits = 0;
    image->pixel_type = 0;
    image->image.clear();
  }
  return true;
}


} // namespace

bool AvatarMeshLoader::load(const QString &path, AvatarMeshData &out,
                            QString *error) {


  auto fail = [&](const QString &reason) {
    if (error) {
      *error = reason;
    }
    qWarning() << "[AvatarMeshLoader]" << reason;
    return false;
  };

  QFile file(path);

  if (!file.open(QIODevice::ReadOnly)) {
    return fail(QStringLiteral("Cannot open %1: %2")
                    .arg(path, file.errorString()));
  }

  const QByteArray bytes = file.readAll();
  file.close();

  if (bytes.isEmpty()) {
    return fail(QStringLiteral("File is empty: %1").arg(path));
  }

  tinygltf::TinyGLTF loader;
  loader.SetImageLoader(loadImageDataNoOp, nullptr);
  tinygltf::Model model;

  std::string err;
  std::string warn;

  loader.SetParseStrictness(tinygltf::ParseStrictness::Permissive);

  const std::string stdPath = path.toStdString();

  const bool ok = loader.LoadBinaryFromMemory(
      &model, &err, &warn,
      reinterpret_cast<const unsigned char *>(bytes.constData()),
      static_cast<unsigned int>(bytes.size()),
      stdPath);

  if (!warn.empty()) {
    qWarning() << "[AvatarMeshLoader] tinygltf warning:"
               << QString::fromStdString(warn);
  }

  if (!ok) {
    return fail(QStringLiteral("tinygltf failed to parse %1: %2")
                    .arg(path, QString::fromStdString(err)));
  }


  if (model.meshes.empty()) {
    return fail(QStringLiteral("GLB has no meshes: %1").arg(path));
  }

  // -----------------------------------------------------------------
  // Textures. The images are read straight out of the buffer views,
  // not through tinygltf's image loader, because the loader is a no-op
  // in this build. The encoded bytes are kept as-is; decoding is the
  // renderer's job.
  // -----------------------------------------------------------------

  out.textures.clear();
  out.textures.reserve(static_cast<int>(model.images.size()));

  for (int i = 0; i < static_cast<int>(model.images.size()); ++i) {
    const tinygltf::Image &image = model.images[static_cast<size_t>(i)];

    AvatarTexture tex;
    tex.name = QString::fromStdString(image.name);
    tex.mimeType = QString::fromStdString(image.mimeType);

    if (image.bufferView >= 0 &&
        image.bufferView < static_cast<int>(model.bufferViews.size())) {
      const tinygltf::BufferView &view =
          model.bufferViews[static_cast<size_t>(image.bufferView)];

      if (view.buffer >= 0 &&
          view.buffer < static_cast<int>(model.buffers.size())) {
        const tinygltf::Buffer &buffer =
            model.buffers[static_cast<size_t>(view.buffer)];

        const size_t start = static_cast<size_t>(view.byteOffset);
        const size_t length = static_cast<size_t>(view.byteLength);

        if (start + length <= buffer.data.size()) {
          tex.bytes = QByteArray(
              reinterpret_cast<const char *>(buffer.data.data() + start),
              static_cast<qsizetype>(length));
        } else {
          qWarning() << "[AvatarMeshLoader] Texture buffer view out of"
                     << "bounds:" << i;
        }
      }
    }

    out.textures.append(tex);
  }

  // Material list. Each material keeps its name and the index of its
  // base color texture, or -1.
  out.materials.clear();
  out.materials.reserve(static_cast<int>(model.materials.size()));

  for (const tinygltf::Material &mat : model.materials) {
    AvatarMaterial outMat;
    outMat.name = QString::fromStdString(mat.name);

    const auto it = mat.pbrMetallicRoughness.baseColorTexture.index;

    if (it >= 0 && it < static_cast<int>(model.textures.size())) {
      const tinygltf::Texture &tex =
          model.textures[static_cast<size_t>(it)];

      if (tex.source >= 0 &&
          tex.source < static_cast<int>(model.images.size())) {
        outMat.baseColorTextureIndex = tex.source;
      }
    }

    if (mat.pbrMetallicRoughness.baseColorFactor.size() == 4) {
      outMat.baseColorFactor = QVector4D(
          static_cast<float>(mat.pbrMetallicRoughness.baseColorFactor[0]),
          static_cast<float>(mat.pbrMetallicRoughness.baseColorFactor[1]),
          static_cast<float>(mat.pbrMetallicRoughness.baseColorFactor[2]),
          static_cast<float>(mat.pbrMetallicRoughness.baseColorFactor[3]));
    }

    out.materials.append(outMat);
  }

  out.materialNames.clear();
  out.materialNames.reserve(out.materials.size());

  for (const AvatarMaterial &mat : out.materials) {
    out.materialNames.append(mat.name);
  }

  qDebug() << "[AvatarMeshLoader] Textures:" << out.textures.size()
           << "materials:" << out.materials.size();

  // -----------------------------------------------------------------
  // Meshes and primitives.
  // -----------------------------------------------------------------

  out.meshes.clear();
  out.meshes.reserve(static_cast<int>(model.meshes.size()));

  for (const tinygltf::Mesh &mesh : model.meshes) {
    AvatarMesh outMesh;
    outMesh.name = QString::fromStdString(mesh.name);

    for (const tinygltf::Primitive &prim : mesh.primitives) {
      AvatarPrimitive outPrim;

      outPrim.materialIndex = prim.material;

      const auto posIt = prim.attributes.find("POSITION");
      const auto nrmIt = prim.attributes.find("NORMAL");
      const auto uvIt = prim.attributes.find("TEXCOORD_0");
      const auto jointsIt = prim.attributes.find("JOINTS_0");
      const auto weightsIt = prim.attributes.find("WEIGHTS_0");

      if (posIt == prim.attributes.end()) {
        qWarning() << "[AvatarMeshLoader] Primitive without POSITION in"
                   << outMesh.name;
        continue;
      }

      if (!readFloatAccessor(model, posIt->second, 3, outPrim.positions)) {
        return fail(QStringLiteral("Failed to read POSITION in %1")
                        .arg(outMesh.name));
      }

      if (nrmIt != prim.attributes.end()) {
        if (!readFloatAccessor(model, nrmIt->second, 3, outPrim.normals)) {
          return fail(QStringLiteral("Failed to read NORMAL in %1")
                          .arg(outMesh.name));
        }
      }

      if (uvIt != prim.attributes.end()) {
        if (!readFloatAccessor(model, uvIt->second, 2, outPrim.uvs)) {
          return fail(QStringLiteral("Failed to read TEXCOORD_0 in %1")
                          .arg(outMesh.name));
        }
      }

      if (jointsIt != prim.attributes.end()) {
        if (!readU16Accessor(model, jointsIt->second, 4,
                             outPrim.jointIndices)) {
          return fail(QStringLiteral("Failed to read JOINTS_0 in %1")
                          .arg(outMesh.name));
        }
      }

      if (weightsIt != prim.attributes.end()) {
        if (!readFloatAccessor(model, weightsIt->second, 4,
                               outPrim.jointWeights)) {
          return fail(QStringLiteral("Failed to read WEIGHTS_0 in %1")
                          .arg(outMesh.name));
        }
      }

      if (prim.indices >= 0) {
        if (!readIndexAccessor(model, prim.indices, outPrim.indices)) {
          return fail(QStringLiteral("Failed to read indices in %1")
                          .arg(outMesh.name));
        }
      }

      const int targetCount = static_cast<int>(prim.targets.size());

      outPrim.morphTargets.reserve(targetCount);

      for (int t = 0; t < targetCount; ++t) {
        const auto &target = prim.targets[static_cast<size_t>(t)];

        const auto deltaIt = target.find("POSITION");

        if (deltaIt == target.end()) {
          outPrim.morphTargets.append(QVector<float>());
          outPrim.morphTargetNames.append(morphTargetName(mesh, prim, t));
          continue;
        }

        QVector<float> deltas;

        if (!readFloatAccessor(model, deltaIt->second, 3, deltas)) {
          return fail(QStringLiteral("Failed to read morph target %1 in %2")
                          .arg(t)
                          .arg(outMesh.name));
        }

        outPrim.morphTargets.append(deltas);
        outPrim.morphTargetNames.append(morphTargetName(mesh, prim, t));
      }

      outMesh.primitives.append(outPrim);
    }

    out.meshes.append(outMesh);
  }

  // -----------------------------------------------------------------
  // Skins.
  // -----------------------------------------------------------------

  out.skins.clear();
  out.skins.reserve(static_cast<int>(model.skins.size()));

  for (const tinygltf::Skin &skin : model.skins) {
    AvatarSkin outSkin;
    outSkin.name = QString::fromStdString(skin.name);

    for (int nodeIndex : skin.joints) {
      if (nodeIndex < 0 ||
          nodeIndex >= static_cast<int>(model.nodes.size())) {
        qWarning() << "[AvatarMeshLoader] Skin joint node index out of"
                   << "range:" << nodeIndex;
        continue;
      }

      outSkin.jointNames.append(
          QString::fromStdString(
              model.nodes[static_cast<size_t>(nodeIndex)].name));
    }

    if (skin.inverseBindMatrices >= 0) {
      if (!readFloatAccessor(model, skin.inverseBindMatrices, 16,
                             outSkin.inverseBindPoses)) {
        return fail(QStringLiteral("Failed to read inverse bind matrices"
                                   " for skin %1")
                        .arg(outSkin.name));
      }
    }

    if (!outSkin.isConsistent()) {
      return fail(QStringLiteral("Skin %1 has %2 joints but %3 floats"
                                 " of inverse bind data (expected %4).")
                      .arg(outSkin.name)
                      .arg(outSkin.jointCount())
                      .arg(outSkin.inverseBindPoses.size())
                      .arg(16 * outSkin.jointCount()));
    }

    out.skins.append(outSkin);

    qDebug() << "[AvatarMeshLoader] Skin" << outSkin.name
             << "joints:" << outSkin.jointCount()
             << "inverse bind floats:" << outSkin.inverseBindPoses.size();
  }

  if (out.skins.isEmpty()) {
    qWarning() << "[AvatarMeshLoader] No skins in the GLB. Skinning"
               << "will not be possible.";
  }

  out.faceMeshIndex = -1;

  for (int i = 0; i < out.meshes.size(); ++i) {
    if (out.meshes.at(i).name == QStringLiteral("CC_Base_Body")) {
      out.faceMeshIndex = i;
      break;
    }
  }

  qDebug() << "[AvatarMeshLoader] Loaded" << out.meshes.size()
           << "meshes," << out.totalPrimitives() << "primitives,"
           << out.totalMorphTargets() << "morph targets,"
           << out.skins.size() << "skins."
           << "Face mesh index:" << out.faceMeshIndex;

  if (out.faceMeshIndex >= 0 &&
      out.faceMeshIndex < out.meshes.size()) {
    const AvatarMesh &face = out.meshes.at(out.faceMeshIndex);

    if (!face.primitives.isEmpty()) {
      const AvatarPrimitive &prim = face.primitives.first();

      qDebug() << "[AvatarMeshLoader] Face mesh first prim morph names:";

      for (int t = 0; t < qMin(8, prim.morphTargetNames.size()); ++t) {
        qDebug() << "    " << t << prim.morphTargetNames.at(t);
      }
    }
  }

  return true;
}