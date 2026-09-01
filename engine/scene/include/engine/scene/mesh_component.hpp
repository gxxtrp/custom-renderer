#pragma once

#include <engine/assets/mesh_asset.hpp>
#include <engine/core/types.hpp>
#include <engine/core/vec3.hpp>

namespace engine::scene {

struct MeshComponent {
  const assets::MeshAsset *meshAsset{nullptr};
  core::u32 meshId{0};
  core::u32 materialId{0};
  core::u32 meshletOffset{0};
  core::u32 meshletCount{0};
  core::Vec3 localAabbMin{-1.0f, -1.0f, -1.0f};
  core::Vec3 localAabbMax{1.0f, 1.0f, 1.0f};
  core::Vec3 localBoundingSphereCenter{0.0f, 0.0f, 0.0f};
  core::f32 localBoundingSphereRadius{1.732f};

  MeshComponent() = default;

  explicit MeshComponent(const assets::MeshAsset *asset, core::u32 id = 0,
                         core::u32 offset = 0)
      : meshAsset(asset), meshId(id), meshletOffset(offset) {
    if (asset != nullptr) {
      meshletCount = static_cast<core::u32>(asset->getMeshletCount());
      localAabbMin = asset->getBoundingBoxMin();
      localAabbMax = asset->getBoundingBoxMax();
      localBoundingSphereCenter = (localAabbMin + localAabbMax) * 0.5f;
      localBoundingSphereRadius =
          (localAabbMax - localBoundingSphereCenter).length();
    }
  }
};

} // namespace engine::scene
