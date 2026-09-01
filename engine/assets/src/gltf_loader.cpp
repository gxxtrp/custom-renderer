#include <engine/assets/gltf_loader.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <numbers>
#include <string>
#include <vector>

#include <engine/core/log.hpp>
#include <fastgltf/core.hpp>
#include <fastgltf/tools.hpp>
#include <fastgltf/types.hpp>
#include <meshoptimizer.h>

namespace engine::assets {

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
