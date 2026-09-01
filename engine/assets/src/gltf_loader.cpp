#include <engine/assets/gltf_loader.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <numbers>
#include <string>
#include <vector>

#include <engine/core/log.hpp>
#include <engine/core/mat4.hpp>
#include <engine/core/quat.hpp>
#include <engine/core/vec3.hpp>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <meshoptimizer.h>

#include "stb_image.h"

namespace engine::assets {

namespace {

core::Mat4 toEngineMat4(const fastgltf::math::fmat4x4 &m) {
  core::Mat4 mat;
  for (core::usize col = 0; col < 4; ++col) {
    for (core::usize row = 0; row < 4; ++row) {
      mat(row, col) = m[col][row];
    }
  }
  return mat;
}

void traverseNodeHierarchy(const fastgltf::Asset &asset, core::usize nodeIndex,
                           const fastgltf::math::fmat4x4 &parentTransform,
                           std::vector<GltfMeshInstance> &outInstances) {
  if (nodeIndex >= asset.nodes.size()) {
    return;
  }

  const auto &node = asset.nodes[nodeIndex];
  const fastgltf::math::fmat4x4 worldTransform =
      fastgltf::getTransformMatrix(node, parentTransform);

  if (node.meshIndex.has_value()) {
    const auto meshIdx = static_cast<core::u32>(node.meshIndex.value());
    if (meshIdx < asset.meshes.size()) {
      const auto &mesh = asset.meshes[meshIdx];
      for (core::usize primIdx = 0; primIdx < mesh.primitives.size();
           ++primIdx) {
        const auto &prim = mesh.primitives[primIdx];
        const core::u32 matIdx =
            prim.materialIndex.has_value()
                ? static_cast<core::u32>(prim.materialIndex.value())
                : 0;

        outInstances.push_back(GltfMeshInstance{
            .meshIndex = meshIdx,
            .materialIndex = matIdx,
            .transform = toEngineMat4(worldTransform),
        });
      }
    }
  }

  for (auto childIdx : node.children) {
    traverseNodeHierarchy(asset, childIdx, worldTransform, outInstances);
  }
}

} // namespace

MeshAsset buildMeshletsFromGeometry(std::span<const Vertex> vertices,
                                    std::span<const core::u32> indices,
                                    const MeshletBuildOptions &options) {
  if (vertices.empty() || indices.empty()) {
    return MeshAsset{};
  }

  core::Vec3 boxMin{vertices[0].position};
  core::Vec3 boxMax{vertices[0].position};
  for (const auto &v : vertices) {
    boxMin.x = std::min(boxMin.x, v.position.x);
    boxMin.y = std::min(boxMin.y, v.position.y);
    boxMin.z = std::min(boxMin.z, v.position.z);
    boxMax.x = std::max(boxMax.x, v.position.x);
    boxMax.y = std::max(boxMax.y, v.position.y);
    boxMax.z = std::max(boxMax.z, v.position.z);
  }

  const core::usize maxMeshlets = meshopt_buildMeshletsBound(
      indices.size(), options.maxVertices, options.maxTriangles);

  std::vector<meshopt_Meshlet> rawMeshlets(maxMeshlets);
  std::vector<core::u32> meshletVertices(maxMeshlets * options.maxVertices);
  std::vector<core::u8> rawTriangles(maxMeshlets * options.maxTriangles * 3);

  const core::usize meshletCount = meshopt_buildMeshlets(
      rawMeshlets.data(), meshletVertices.data(), rawTriangles.data(),
      indices.data(), indices.size(), &vertices[0].position.x, vertices.size(),
      sizeof(Vertex), options.maxVertices, options.maxTriangles,
      options.coneWeight);

  rawMeshlets.resize(meshletCount);

  std::vector<Meshlet> meshlets;
  meshlets.reserve(meshletCount);

  std::vector<core::u32> packedTriangles;
  std::vector<core::u32> globalIndices;

  for (core::u32 i = 0; i < static_cast<core::u32>(meshletCount); ++i) {
    const auto &raw = rawMeshlets[i];

    meshopt_optimizeMeshlet(&meshletVertices[raw.vertex_offset],
                            &rawTriangles[raw.triangle_offset],
                            raw.triangle_count, raw.vertex_count);

    const meshopt_Bounds bounds = meshopt_computeMeshletBounds(
        &meshletVertices[raw.vertex_offset], &rawTriangles[raw.triangle_offset],
        raw.triangle_count, &vertices[0].position.x, vertices.size(),
        sizeof(Vertex));

    Meshlet m{};
    m.center[0] = bounds.center[0];
    m.center[1] = bounds.center[1];
    m.center[2] = bounds.center[2];
    m.radius = bounds.radius;

    m.coneAxis[0] = bounds.cone_axis[0];
    m.coneAxis[1] = bounds.cone_axis[1];
    m.coneAxis[2] = bounds.cone_axis[2];
    m.coneCutoff = bounds.cone_cutoff;

    m.coneApex[0] = bounds.cone_apex[0];
    m.coneApex[1] = bounds.cone_apex[1];
    m.coneApex[2] = bounds.cone_apex[2];

    m.meshletId = i;
    m.vertexOffset = raw.vertex_offset;
    m.vertexCount = raw.vertex_count;
    m.primitiveOffset = static_cast<core::u32>(packedTriangles.size());
    m.primitiveCount = raw.triangle_count;

    meshlets.push_back(m);

    for (core::u32 t = 0; t < raw.triangle_count; ++t) {
      const auto i0 =
          static_cast<core::u32>(rawTriangles[raw.triangle_offset + t * 3 + 0]);
      const auto i1 =
          static_cast<core::u32>(rawTriangles[raw.triangle_offset + t * 3 + 1]);
      const auto i2 =
          static_cast<core::u32>(rawTriangles[raw.triangle_offset + t * 3 + 2]);

      packedTriangles.push_back(i0 | (i1 << 8) | (i2 << 16));

      const core::u32 g0 = meshletVertices[raw.vertex_offset + i0];
      const core::u32 g1 = meshletVertices[raw.vertex_offset + i1];
      const core::u32 g2 = meshletVertices[raw.vertex_offset + i2];
      globalIndices.push_back(g0);
      globalIndices.push_back(g1);
      globalIndices.push_back(g2);
    }
  }

  core::usize totalVerticesUsed = 0;
  for (const auto &raw : rawMeshlets) {
    totalVerticesUsed =
        std::max(totalVerticesUsed, static_cast<core::usize>(raw.vertex_offset +
                                                             raw.vertex_count));
  }
  meshletVertices.resize(totalVerticesUsed);

  std::vector<Vertex> vertexData(vertices.begin(), vertices.end());

  ENGINE_LOG_INFO("Meshlet clustering complete: {} vertices, {} triangles "
                  "clustered into {} meshlets (packed triangles: {}, "
                  "global indices: {})",
                  vertexData.size(), globalIndices.size() / 3, meshlets.size(),
                  packedTriangles.size(), globalIndices.size());

  return MeshAsset(std::move(vertexData), std::move(meshlets),
                   std::move(meshletVertices), std::move(packedTriangles),
                   std::move(globalIndices), boxMin, boxMax);
}

std::expected<MeshAsset, std::string>
loadGltfMesh(const std::filesystem::path &path,
             const MeshletBuildOptions &options) {
  if (!std::filesystem::exists(path)) {
    return std::unexpected("File does not exist: " + path.string());
  }

  auto dataBuffer = fastgltf::GltfDataBuffer::FromPath(path);
  if (dataBuffer.error() != fastgltf::Error::None) {
    return std::unexpected("Failed to open glTF file: " + path.string());
  }

  fastgltf::Parser parser;
  constexpr auto gltfOptions = fastgltf::Options::LoadExternalBuffers;
  auto parsed =
      parser.loadGltf(dataBuffer.get(), path.parent_path(), gltfOptions);
  if (parsed.error() != fastgltf::Error::None) {
    return std::unexpected(
        "Failed to parse glTF error code: " +
        std::to_string(fastgltf::to_underlying(parsed.error())));
  }

  const fastgltf::Asset &asset = parsed.get();
  if (asset.meshes.empty()) {
    return std::unexpected("glTF contains no meshes: " + path.string());
  }

  std::vector<Vertex> vertices;
  std::vector<core::u32> indices;

  for (const auto &mesh : asset.meshes) {
    for (const auto &prim : mesh.primitives) {
      if (prim.type != fastgltf::PrimitiveType::Triangles) {
        continue;
      }

      const auto posIt = prim.findAttribute("POSITION");
      if (posIt == prim.attributes.end()) {
        continue;
      }

      const auto baseVertex = static_cast<core::u32>(vertices.size());
      const auto &posAccessor = asset.accessors[posIt->accessorIndex];
      const core::usize primVertexCount = posAccessor.count;

      std::vector<Vertex> primVertices(primVertexCount);
      fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
          asset, posAccessor, [&](fastgltf::math::fvec3 pos, std::size_t idx) {
            primVertices[idx].position = core::Vec3(pos.x(), pos.y(), pos.z());
          });

      const auto normIt = prim.findAttribute("NORMAL");
      if (normIt != prim.attributes.end()) {
        const auto &normAccessor = asset.accessors[normIt->accessorIndex];
        fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
            asset, normAccessor,
            [&](fastgltf::math::fvec3 norm, std::size_t idx) {
              primVertices[idx].normal =
                  core::Vec3(norm.x(), norm.y(), norm.z());
            });
      }

      const auto uvIt = prim.findAttribute("TEXCOORD_0");
      if (uvIt != prim.attributes.end()) {
        const auto &uvAccessor = asset.accessors[uvIt->accessorIndex];
        fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(
            asset, uvAccessor, [&](fastgltf::math::fvec2 uv, std::size_t idx) {
              primVertices[idx].uv = core::Vec2(uv.x(), uv.y());
            });
      }

      vertices.insert(vertices.end(), primVertices.begin(), primVertices.end());

      if (prim.indicesAccessor.has_value()) {
        const auto &indexAccessor =
            asset.accessors[prim.indicesAccessor.value()];
        fastgltf::iterateAccessorWithIndex<core::u32>(
            asset, indexAccessor, [&](core::u32 idx, std::size_t /*i*/) {
              indices.push_back(baseVertex + idx);
            });
      } else {
        for (core::u32 i = 0; i < static_cast<core::u32>(primVertexCount);
             ++i) {
          indices.push_back(baseVertex + i);
        }
      }
    }
  }

  if (vertices.empty() || indices.empty()) {
    return std::unexpected("No valid triangles found in glTF file: " +
                           path.string());
  }

  ENGINE_LOG_INFO("Loaded glTF mesh '{}': {} vertices, {} indices",
                  path.string(), vertices.size(), indices.size());
  return buildMeshletsFromGeometry(vertices, indices, options);
}

std::expected<GltfSceneAsset, std::string>
loadGltfScene(const std::filesystem::path &path,
              const MeshletBuildOptions &options) {
  if (!std::filesystem::exists(path)) {
    return std::unexpected("File does not exist: " + path.string());
  }

  auto dataBuffer = fastgltf::GltfDataBuffer::FromPath(path);
  if (dataBuffer.error() != fastgltf::Error::None) {
    return std::unexpected("Failed to open glTF file: " + path.string());
  }

  fastgltf::Parser parser;
  constexpr auto gltfOptions = fastgltf::Options::LoadExternalBuffers |
                               fastgltf::Options::LoadExternalImages |
                               fastgltf::Options::DecomposeNodeMatrices;
  auto parsed =
      parser.loadGltf(dataBuffer.get(), path.parent_path(), gltfOptions);
  if (parsed.error() != fastgltf::Error::None) {
    return std::unexpected(
        "Failed to parse glTF error code: " +
        std::to_string(fastgltf::to_underlying(parsed.error())));
  }

  const fastgltf::Asset &asset = parsed.get();
  GltfSceneAsset sceneAsset;

  // 1. Decode Images into TextureAsset
  std::vector<core::u32> imageToTextureMap(asset.images.size(), 0);
  for (core::usize imgIdx = 0; imgIdx < asset.images.size(); ++imgIdx) {
    const auto &image = asset.images[imgIdx];
    int width = 0;
    int height = 0;
    int channels = 0;
    stbi_uc *pixelData = nullptr;

    std::visit(fastgltf::visitor{
                   [&](const fastgltf::sources::URI &uri) {
                     const auto fullPath =
                         path.parent_path() / std::string(uri.uri.path());
                     pixelData = stbi_load(fullPath.string().c_str(), &width,
                                           &height, &channels, 4);
                   },
                   [&](const fastgltf::sources::Vector &vec) {
                     pixelData = stbi_load_from_memory(
                         reinterpret_cast<const stbi_uc *>(vec.bytes.data()),
                         static_cast<int>(vec.bytes.size()), &width, &height,
                         &channels, 4);
                   },
                   [&](const fastgltf::sources::Array &arr) {
                     pixelData = stbi_load_from_memory(
                         reinterpret_cast<const stbi_uc *>(arr.bytes.data()),
                         static_cast<int>(arr.bytes.size()), &width, &height,
                         &channels, 4);
                   },
                   [&](const fastgltf::sources::BufferView &view) {
                     if (view.bufferViewIndex < asset.bufferViews.size()) {
                       const auto &bv = asset.bufferViews[view.bufferViewIndex];
                       if (bv.bufferIndex < asset.buffers.size()) {
                         const auto &buf = asset.buffers[bv.bufferIndex];
                         std::visit(
                             fastgltf::visitor{
                                 [&](const fastgltf::sources::Vector &vec) {
                                   if (bv.byteOffset + bv.byteLength <=
                                       vec.bytes.size()) {
                                     const auto *ptr =
                                         reinterpret_cast<const stbi_uc *>(
                                             vec.bytes.data() + bv.byteOffset);
                                     pixelData = stbi_load_from_memory(
                                         ptr, static_cast<int>(bv.byteLength),
                                         &width, &height, &channels, 4);
                                   }
                                 },
                                 [&](const fastgltf::sources::Array &arr) {
                                   if (bv.byteOffset + bv.byteLength <=
                                       arr.bytes.size()) {
                                     const auto *ptr =
                                         reinterpret_cast<const stbi_uc *>(
                                             arr.bytes.data() + bv.byteOffset);
                                     pixelData = stbi_load_from_memory(
                                         ptr, static_cast<int>(bv.byteLength),
                                         &width, &height, &channels, 4);
                                   }
                                 },
                                 [&](const auto &) {}},
                             buf.data);
                       }
                     }
                   },
                   [&](const auto &) {}},
               image.data);

    if (pixelData != nullptr && width > 0 && height > 0) {
      std::vector<core::u8> rawBytes(pixelData,
                                     pixelData + (width * height * 4));
      stbi_image_free(pixelData);

      const auto textureIdx =
          static_cast<core::u32>(sceneAsset.textures.size());
      sceneAsset.textures.emplace_back(
          width, height, 4, rhi::Format::RGBA8_UNORM, std::move(rawBytes));
      imageToTextureMap[imgIdx] = textureIdx;
    } else {
      ENGINE_LOG_WARN("Failed to load image {} in glTF: {}", imgIdx,
                      image.name);
    }
  }

  auto getTextureIndex = [&](std::optional<core::usize> texIdx) -> core::u32 {
    if (!texIdx.has_value() || texIdx.value() >= asset.textures.size()) {
      return 0;
    }
    const auto &tex = asset.textures[texIdx.value()];
    if (!tex.imageIndex.has_value() ||
        tex.imageIndex.value() >= imageToTextureMap.size()) {
      return 0;
    }
    return imageToTextureMap[tex.imageIndex.value()];
  };

  // 2. Parse Materials
  for (const auto &gltfMat : asset.materials) {
    MaterialAsset mat{};
    mat.baseColorFactor = core::Vec4(
        gltfMat.pbrData.baseColorFactor[0], gltfMat.pbrData.baseColorFactor[1],
        gltfMat.pbrData.baseColorFactor[2], gltfMat.pbrData.baseColorFactor[3]);
    mat.metallicFactor = gltfMat.pbrData.metallicFactor;
    mat.roughnessFactor = gltfMat.pbrData.roughnessFactor;
    mat.emissiveFactor =
        core::Vec3(gltfMat.emissiveFactor[0], gltfMat.emissiveFactor[1],
                   gltfMat.emissiveFactor[2]);
    mat.alphaCutoff = gltfMat.alphaCutoff;

    core::u32 flags = 0;
    if (gltfMat.doubleSided) {
      flags |= static_cast<core::u32>(MaterialFlags::DoubleSided);
    }
    if (gltfMat.alphaMode == fastgltf::AlphaMode::Blend) {
      flags |= static_cast<core::u32>(MaterialFlags::AlphaBlend);
    } else if (gltfMat.alphaMode == fastgltf::AlphaMode::Mask) {
      flags |= static_cast<core::u32>(MaterialFlags::AlphaMask);
    }
    mat.flags = flags;

    if (gltfMat.pbrData.baseColorTexture.has_value()) {
      mat.baseColorTextureIndex =
          getTextureIndex(gltfMat.pbrData.baseColorTexture->textureIndex);
    }
    if (gltfMat.normalTexture.has_value()) {
      mat.normalTextureIndex =
          getTextureIndex(gltfMat.normalTexture->textureIndex);
      mat.normalScale = gltfMat.normalTexture->scale;
    }
    if (gltfMat.pbrData.metallicRoughnessTexture.has_value()) {
      mat.metallicRoughnessTextureIndex = getTextureIndex(
          gltfMat.pbrData.metallicRoughnessTexture->textureIndex);
    }
    if (gltfMat.emissiveTexture.has_value()) {
      mat.emissiveTextureIndex =
          getTextureIndex(gltfMat.emissiveTexture->textureIndex);
    }
    if (gltfMat.occlusionTexture.has_value()) {
      mat.occlusionTextureIndex =
          getTextureIndex(gltfMat.occlusionTexture->textureIndex);
      mat.occlusionStrength = gltfMat.occlusionTexture->strength;
    }

    sceneAsset.materials.push_back(mat);
  }

  // Ensure at least one material
  if (sceneAsset.materials.empty()) {
    sceneAsset.materials.push_back(MaterialAsset{});
  }

  // 3. Parse Meshes
  for (const auto &mesh : asset.meshes) {
    std::vector<Vertex> vertices;
    std::vector<core::u32> indices;

    for (const auto &prim : mesh.primitives) {
      if (prim.type != fastgltf::PrimitiveType::Triangles) {
        continue;
      }

      const auto posIt = prim.findAttribute("POSITION");
      if (posIt == prim.attributes.end()) {
        continue;
      }

      const auto baseVertex = static_cast<core::u32>(vertices.size());
      const auto &posAccessor = asset.accessors[posIt->accessorIndex];
      const core::usize primVertexCount = posAccessor.count;

      std::vector<Vertex> primVertices(primVertexCount);
      fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
          asset, posAccessor, [&](fastgltf::math::fvec3 pos, std::size_t idx) {
            primVertices[idx].position = core::Vec3(pos.x(), pos.y(), pos.z());
          });

      const auto normIt = prim.findAttribute("NORMAL");
      if (normIt != prim.attributes.end()) {
        const auto &normAccessor = asset.accessors[normIt->accessorIndex];
        fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(
            asset, normAccessor,
            [&](fastgltf::math::fvec3 norm, std::size_t idx) {
              primVertices[idx].normal =
                  core::Vec3(norm.x(), norm.y(), norm.z());
            });
      }

      const auto uvIt = prim.findAttribute("TEXCOORD_0");
      if (uvIt != prim.attributes.end()) {
        const auto &uvAccessor = asset.accessors[uvIt->accessorIndex];
        fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(
            asset, uvAccessor, [&](fastgltf::math::fvec2 uv, std::size_t idx) {
              primVertices[idx].uv = core::Vec2(uv.x(), uv.y());
            });
      }

      vertices.insert(vertices.end(), primVertices.begin(), primVertices.end());

      if (prim.indicesAccessor.has_value()) {
        const auto &indexAccessor =
            asset.accessors[prim.indicesAccessor.value()];
        fastgltf::iterateAccessorWithIndex<core::u32>(
            asset, indexAccessor, [&](core::u32 idx, std::size_t /*i*/) {
              indices.push_back(baseVertex + idx);
            });
      } else {
        for (core::u32 i = 0; i < static_cast<core::u32>(primVertexCount);
             ++i) {
          indices.push_back(baseVertex + i);
        }
      }
    }

    if (!vertices.empty() && !indices.empty()) {
      sceneAsset.meshes.push_back(
          buildMeshletsFromGeometry(vertices, indices, options));
    }
  }

  // 4. Parse Nodes / Instances from default scene
  const auto sceneIndex = asset.defaultScene.has_value()
                              ? asset.defaultScene.value()
                              : (asset.scenes.empty() ? 0 : 0);
  if (!asset.scenes.empty() && sceneIndex < asset.scenes.size()) {
    const auto &scene = asset.scenes[sceneIndex];
    for (auto nodeIdx : scene.nodeIndices) {
      traverseNodeHierarchy(asset, nodeIdx, fastgltf::math::fmat4x4(),
                            sceneAsset.instances);
    }
  }

  // If no instances extracted from node hierarchy, create a default instance
  // per mesh
  if (sceneAsset.instances.empty()) {
    for (core::u32 i = 0; i < static_cast<core::u32>(sceneAsset.meshes.size());
         ++i) {
      sceneAsset.instances.push_back(GltfMeshInstance{
          .meshIndex = i,
          .materialIndex = i < sceneAsset.materials.size() ? i : 0,
          .transform = core::Mat4::identity(),
      });
    }
  }

  ENGINE_LOG_INFO(
      "Loaded glTF scene '{}': {} meshes, {} materials, {} textures, "
      "{} instances",
      path.string(), sceneAsset.meshes.size(), sceneAsset.materials.size(),
      sceneAsset.textures.size(), sceneAsset.instances.size());

  return sceneAsset;
}

MeshAsset createProceduralKnot(core::u32 slices, core::u32 stacks,
                               const MeshletBuildOptions &options) {
  std::vector<Vertex> vertices;
  std::vector<core::u32> indices;

  vertices.reserve((slices + 1) * (stacks + 1));
  indices.reserve(slices * stacks * 6);

  constexpr core::f32 pi = std::numbers::pi_v<core::f32>;
  constexpr core::f32 twoPi = 2.0f * pi;
  constexpr core::f32 tubeRadius = 0.35f;

  auto knotPoint = [](core::f32 t) {
    constexpr core::f32 p = 2.0f;
    constexpr core::f32 q = 3.0f;
    const core::f32 r = 0.5f * (2.0f + std::sin(q * t));
    const core::f32 x = r * std::cos(p * t);
    const core::f32 y = r * std::sin(p * t);
    const core::f32 z = 0.35f * std::cos(q * t);
    return core::Vec3(x, y, z);
  };

  for (core::u32 i = 0; i <= slices; ++i) {
    const core::f32 u =
        static_cast<core::f32>(i) / static_cast<core::f32>(slices);
    const core::f32 t = u * twoPi;

    const core::Vec3 p = knotPoint(t);
    const core::Vec3 pNext = knotPoint(t + 0.001f);
    const core::Vec3 T = (pNext - p).normalized();
    core::Vec3 N = (pNext + knotPoint(t - 0.001f) - p * 2.0f).normalized();
    if (N.lengthSquared() < 0.001f) {
      N = core::Vec3(0.0f, 1.0f, 0.0f);
    }
    const core::Vec3 B = T.cross(N).normalized();
    N = B.cross(T).normalized();

    for (core::u32 j = 0; j <= stacks; ++j) {
      const core::f32 v =
          static_cast<core::f32>(j) / static_cast<core::f32>(stacks);
      const core::f32 theta = v * twoPi;

      const core::f32 cosTheta = std::cos(theta);
      const core::f32 sinTheta = std::sin(theta);

      const core::Vec3 normal = (N * cosTheta + B * sinTheta).normalized();
      const core::Vec3 pos = p + normal * tubeRadius;

      vertices.push_back(
          Vertex{.position = pos, .normal = normal, .uv = core::Vec2(u, v)});
    }
  }

  for (core::u32 i = 0; i < slices; ++i) {
    for (core::u32 j = 0; j < stacks; ++j) {
      const core::u32 i0 = i * (stacks + 1) + j;
      const core::u32 i1 = (i + 1) * (stacks + 1) + j;
      const core::u32 i2 = (i + 1) * (stacks + 1) + (j + 1);
      const core::u32 i3 = i * (stacks + 1) + (j + 1);

      indices.push_back(i0);
      indices.push_back(i1);
      indices.push_back(i2);

      indices.push_back(i0);
      indices.push_back(i2);
      indices.push_back(i3);
    }
  }

  ENGINE_LOG_INFO("Generated procedural torus knot: {} vertices, {} indices",
                  vertices.size(), indices.size());
  return buildMeshletsFromGeometry(vertices, indices, options);
}

} // namespace engine::assets
