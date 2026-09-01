#pragma once

#include <span>
#include <vector>

#include <engine/assets/meshlet.hpp>
#include <engine/assets/vertex.hpp>
#include <engine/core/types.hpp>
#include <engine/core/vec3.hpp>

namespace engine::assets {

class MeshAsset {
public:
  MeshAsset() = default;
  ~MeshAsset() = default;

  MeshAsset(const MeshAsset &) = default;
  MeshAsset &operator=(const MeshAsset &) = default;

  MeshAsset(MeshAsset &&) noexcept = default;
  MeshAsset &operator=(MeshAsset &&) noexcept = default;

  MeshAsset(std::vector<Vertex> vertices, std::vector<Meshlet> meshlets,
            std::vector<core::u32> meshletVertices,
            std::vector<core::u32> meshletTriangles,
            std::vector<core::u32> globalIndices,
            core::Vec3 boundingBoxMin = {}, core::Vec3 boundingBoxMax = {});

  [[nodiscard]] std::span<const Vertex> getVertices() const noexcept {
    return m_vertices;
  }
  [[nodiscard]] std::span<const Meshlet> getMeshlets() const noexcept {
    return m_meshlets;
  }
  [[nodiscard]] std::span<const core::u32> getMeshletVertices() const noexcept {
    return m_meshletVertices;
  }
  [[nodiscard]] std::span<const core::u32>
  getMeshletTriangles() const noexcept {
    return m_meshletTriangles;
  }
  [[nodiscard]] std::span<const core::u32> getGlobalIndices() const noexcept {
    return m_globalIndices;
  }

  [[nodiscard]] core::usize getVertexCount() const noexcept {
    return m_vertices.size();
  }
  [[nodiscard]] core::usize getMeshletCount() const noexcept {
    return m_meshlets.size();
  }
  [[nodiscard]] core::usize getTriangleCount() const noexcept {
    return m_globalIndices.size() / 3;
  }

  [[nodiscard]] core::Vec3 getBoundingBoxMin() const noexcept {
    return m_boundingBoxMin;
  }
  [[nodiscard]] core::Vec3 getBoundingBoxMax() const noexcept {
    return m_boundingBoxMax;
  }

private:
  std::vector<Vertex> m_vertices;
  std::vector<Meshlet> m_meshlets;
  std::vector<core::u32> m_meshletVertices;
  std::vector<core::u32> m_meshletTriangles;
  std::vector<core::u32> m_globalIndices;

  core::Vec3 m_boundingBoxMin{0.0f, 0.0f, 0.0f};
  core::Vec3 m_boundingBoxMax{0.0f, 0.0f, 0.0f};
};

} // namespace engine::assets
