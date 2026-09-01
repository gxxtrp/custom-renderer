#include <engine/assets/asset_registry.hpp>

#include <utility>

namespace engine::assets {

AssetRegistry::AssetRegistry() {
  m_textures.reserve(64);
  m_materials.reserve(64);
  m_meshes.reserve(64);

  // Slot 0: 1x1 White (for BaseColor, Occlusion, etc.)
  m_textures.push_back(TextureAsset::create1x1Color(255, 255, 255, 255,
                                                    rhi::Format::RGBA8_SRGB));

  // Slot 1: 1x1 Flat Normal (128, 128, 255, 255) -> Normal Vector (0, 0, 1)
  m_textures.push_back(TextureAsset::create1x1Color(128, 128, 255, 255,
                                                    rhi::Format::RGBA8_UNORM));

  // Slot 2: 1x1 Default Metallic-Roughness (255, 255, 255, 255) ->
  // Roughness 1.0, Metallic 1.0
  m_textures.push_back(TextureAsset::create1x1Color(255, 255, 255, 255,
                                                    rhi::Format::RGBA8_UNORM));

  // Slot 3: 1x1 Black (for Emissive)
  m_textures.push_back(
      TextureAsset::create1x1Color(0, 0, 0, 255, rhi::Format::RGBA8_SRGB));

  // Default Material
  MaterialAsset defaultMat{};
  defaultMat.baseColorFactor = core::Vec4(1.0f, 1.0f, 1.0f, 1.0f);
  defaultMat.emissiveFactor = core::Vec3(0.0f, 0.0f, 0.0f);
  defaultMat.metallicFactor = 1.0f;
  defaultMat.roughnessFactor = 1.0f;
  defaultMat.normalScale = 1.0f;
  defaultMat.occlusionStrength = 1.0f;
  defaultMat.alphaCutoff = 0.5f;
  defaultMat.baseColorTextureIndex = DefaultWhiteTextureIndex;
  defaultMat.normalTextureIndex = DefaultNormalTextureIndex;
  defaultMat.metallicRoughnessTextureIndex =
      DefaultMetallicRoughnessTextureIndex;
  defaultMat.emissiveTextureIndex = DefaultBlackTextureIndex;
  defaultMat.occlusionTextureIndex = DefaultWhiteTextureIndex;
  m_materials.push_back(defaultMat);
}

core::u32 AssetRegistry::registerTexture(TextureAsset texture) {
  const auto id = static_cast<core::u32>(m_textures.size());
  m_textures.push_back(std::move(texture));
  return id;
}

core::u32 AssetRegistry::registerMaterial(MaterialAsset material) {
  const auto id = static_cast<core::u32>(m_materials.size());
  m_materials.push_back(std::move(material));
  return id;
}

core::u32 AssetRegistry::registerMesh(MeshAsset mesh) {
  const auto id = static_cast<core::u32>(m_meshes.size());
  m_meshes.push_back(std::move(mesh));
  return id;
}

const TextureAsset *AssetRegistry::getTexture(core::u32 id) const noexcept {
  if (id < m_textures.size()) {
    return &m_textures[id];
  }
  return nullptr;
}

const MaterialAsset *AssetRegistry::getMaterial(core::u32 id) const noexcept {
  if (id < m_materials.size()) {
    return &m_materials[id];
  }
  return nullptr;
}

const MeshAsset *AssetRegistry::getMesh(core::u32 id) const noexcept {
  if (id < m_meshes.size()) {
    return &m_meshes[id];
  }
  return nullptr;
}

} // namespace engine::assets
