#pragma once

#include <expected>
#include <filesystem>
#include <span>
#include <string>

#include <engine/assets/mesh_asset.hpp>

namespace engine::assets {

struct MeshletBuildOptions {
  core::usize maxVertices{64};
  core::usize maxTriangles{124};
  core::f32 coneWeight{0.0f};
};

[[nodiscard]] MeshAsset
buildMeshletsFromGeometry(std::span<const Vertex> vertices,
                          std::span<const core::u32> indices,
                          const MeshletBuildOptions &options = {});

[[nodiscard]] std::expected<MeshAsset, std::string>
loadGltfMesh(const std::filesystem::path &path,
             const MeshletBuildOptions &options = {});

[[nodiscard]] MeshAsset
createProceduralKnot(core::u32 slices = 128, core::u32 stacks = 32,
                     const MeshletBuildOptions &options = {});

} // namespace engine::assets
