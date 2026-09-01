#pragma once

#include <memory>
#include <span>
#include <string>
#include <vector>

#include <engine/assets/material_asset.hpp>
#include <engine/assets/mesh_asset.hpp>
#include <engine/assets/texture_asset.hpp>
#include <engine/core/types.hpp>

namespace engine::assets {

class AssetRegistry {
public:
  AssetRegistry();
  ~AssetRegistry() = default;

  AssetRegistry(const AssetRegistry &) = delete;
  AssetRegistry &operator=(const AssetRegistry &) = delete;

  AssetRegistry(AssetRegistry &&) noexcept = default;
  AssetRegistry &operator=(AssetRegistry &&) noexcept = default;

  [[nodiscard]] core::u32 registerTexture(TextureAsset texture);
  [[nodiscard]] core::u32 registerMaterial(MaterialAsset material);
  [[nodiscard]] core::u32 registerMesh(MeshAsset mesh);

  [[nodiscard]] const TextureAsset *getTexture(core::u32 id) const noexcept;
  [[nodiscard]] const MaterialAsset *getMaterial(core::u32 id) const noexcept;
  [[nodiscard]] const MeshAsset *getMesh(core::u32 id) const noexcept;

  [[nodiscard]] std::span<const TextureAsset> getTextures() const noexcept {
    return m_textures;
  }
  [[nodiscard]] std::span<const MaterialAsset> getMaterials() const noexcept {
    return m_materials;
  }
  [[nodiscard]] std::span<const MeshAsset> getMeshes() const noexcept {
    return m_meshes;
  }

  // Predefined default texture slot indices
  static constexpr core::u32 DefaultWhiteTextureIndex = 0;
  static constexpr core::u32 DefaultNormalTextureIndex = 1;
  static constexpr core::u32 DefaultMetallicRoughnessTextureIndex = 2;
  static constexpr core::u32 DefaultBlackTextureIndex = 3;

  // Predefined default material slot index
  static constexpr core::u32 DefaultMaterialIndex = 0;

private:
  std::vector<TextureAsset> m_textures;
  std::vector<MaterialAsset> m_materials;
  std::vector<MeshAsset> m_meshes;
};

} // namespace engine::assets
