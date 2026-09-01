#pragma once

#include <expected>
#include <filesystem>
#include <span>
#include <string>
#include <vector>

#include <engine/assets/material_asset.hpp>
#include <engine/assets/mesh_asset.hpp>
#include <engine/assets/texture_asset.hpp>
#include <engine/core/mat4.hpp>

namespace engine::assets {

struct MeshletBuildOptions {
  core::usize maxVertices{64};
  core::usize maxTriangles{124};
  core::f32 coneWeight{0.0f};
};

struct GltfMeshInstance {
  core::u32 meshIndex{0};
  core::u32 materialIndex{0};
  core::Mat4 transform{core::Mat4::identity()};
};

struct GltfSceneAsset {
  std::vector<MeshAsset> meshes;
  std::vector<MaterialAsset> materials;
  std::vector<TextureAsset> textures;
  std::vector<GltfMeshInstance> instances;
};

[[nodiscard]] MeshAsset
buildMeshletsFromGeometry(std::span<const Vertex> vertices,
                          std::span<const core::u32> indices,
                          const MeshletBuildOptions &options = {});

[[nodiscard]] std::expected<MeshAsset, std::string>
loadGltfMesh(const std::filesystem::path &path,
             const MeshletBuildOptions &options = {});

[[nodiscard]] std::expected<GltfSceneAsset, std::string>
loadGltfScene(const std::filesystem::path &path,
              const MeshletBuildOptions &options = {});

[[nodiscard]] MeshAsset
createProceduralKnot(core::u32 slices = 128, core::u32 stacks = 32,
                     const MeshletBuildOptions &options = {});

} // namespace engine::assets
