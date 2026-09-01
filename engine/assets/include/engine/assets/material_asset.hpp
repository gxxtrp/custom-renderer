#pragma once

#include <engine/core/types.hpp>
#include <engine/core/vec3.hpp>
#include <engine/core/vec4.hpp>

namespace engine::assets {

enum class MaterialFlags : core::u32 {
  None = 0,
  DoubleSided = 1 << 0,
  AlphaBlend = 1 << 1,
  AlphaMask = 1 << 2,
};

struct MaterialAsset {
  core::Vec4 baseColorFactor{1.0f, 1.0f, 1.0f, 1.0f};
  core::Vec3 emissiveFactor{0.0f, 0.0f, 0.0f};
  core::f32 metallicFactor{1.0f};
  core::f32 roughnessFactor{1.0f};
  core::f32 normalScale{1.0f};
  core::f32 occlusionStrength{1.0f};
  core::f32 alphaCutoff{0.5f};
  core::u32 flags{0};

  // Texture indices into bindless texture table
  core::u32 baseColorTextureIndex{0};
  core::u32 normalTextureIndex{1};
  core::u32 metallicRoughnessTextureIndex{2};
  core::u32 emissiveTextureIndex{3};
  core::u32 occlusionTextureIndex{0};
};

struct alignas(16) GpuMaterialData {
  core::Vec4 baseColorFactor{1.0f, 1.0f, 1.0f, 1.0f}; // 16 bytes
  core::Vec4 emissiveAndAlphaCutoff{
      0.0f, 0.0f, 0.0f, 0.5f};     // xyz = emissive, w = alphaCutoff (16 bytes)
  core::f32 metallicFactor{1.0f};  // 4 bytes
  core::f32 roughnessFactor{1.0f}; // 4 bytes
  core::f32 normalScale{1.0f};     // 4 bytes
  core::f32 occlusionStrength{1.0f};          // 4 bytes (total 16 bytes)
  core::u32 baseColorTextureIndex{0};         // 4 bytes
  core::u32 normalTextureIndex{1};            // 4 bytes
  core::u32 metallicRoughnessTextureIndex{2}; // 4 bytes
  core::u32 emissiveTextureIndex{3};          // 4 bytes (total 16 bytes)

  [[nodiscard]] static GpuMaterialData
  fromMaterialAsset(const MaterialAsset &mat) noexcept {
    return GpuMaterialData{
        .baseColorFactor = mat.baseColorFactor,
        .emissiveAndAlphaCutoff =
            core::Vec4(mat.emissiveFactor.x, mat.emissiveFactor.y,
                       mat.emissiveFactor.z, mat.alphaCutoff),
        .metallicFactor = mat.metallicFactor,
        .roughnessFactor = mat.roughnessFactor,
        .normalScale = mat.normalScale,
        .occlusionStrength = mat.occlusionStrength,
        .baseColorTextureIndex = mat.baseColorTextureIndex,
        .normalTextureIndex = mat.normalTextureIndex,
        .metallicRoughnessTextureIndex = mat.metallicRoughnessTextureIndex,
        .emissiveTextureIndex = mat.emissiveTextureIndex,
    };
  }
};

static_assert(
    sizeof(GpuMaterialData) == 64,
    "GpuMaterialData layout must occupy exactly 64 bytes (1 cache line)");

} // namespace engine::assets
