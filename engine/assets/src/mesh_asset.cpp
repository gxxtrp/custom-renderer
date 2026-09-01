#include <engine/assets/mesh_asset.hpp>

#include <utility>

namespace engine::assets {

MeshAsset::MeshAsset(std::vector<Vertex> vertices,
                     std::vector<Meshlet> meshlets,
                     std::vector<core::u32> meshletVertices,
                     std::vector<core::u32> meshletTriangles,
                     std::vector<core::u32> globalIndices,
                     core::Vec3 boundingBoxMin, core::Vec3 boundingBoxMax)
    : m_vertices(std::move(vertices)), m_meshlets(std::move(meshlets)),
      m_meshletVertices(std::move(meshletVertices)),
      m_meshletTriangles(std::move(meshletTriangles)),
      m_globalIndices(std::move(globalIndices)),
      m_boundingBoxMin(boundingBoxMin), m_boundingBoxMax(boundingBoxMax) {}

} // namespace engine::assets
