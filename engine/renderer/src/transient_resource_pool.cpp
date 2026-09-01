#include <engine/renderer/transient_resource_pool.hpp>

#include <engine/core/log.hpp>

namespace engine::renderer {

TransientResourcePool::~TransientResourcePool() = default;

rhi::Texture *TransientResourcePool::acquireTexture(rhi::Device &device,
                                                    const RGTextureDesc &desc) {
  for (auto &pooled : m_textures) {
    if (!pooled.inUse && pooled.desc.width == desc.width &&
        pooled.desc.height == desc.height && pooled.desc.depth == desc.depth &&
        pooled.desc.mipLevels == desc.mipLevels &&
        pooled.desc.arrayLayers == desc.arrayLayers &&
        pooled.desc.format == desc.format && pooled.desc.usage == desc.usage &&
        pooled.desc.dimension == desc.dimension) {
      pooled.inUse = true;
      return pooled.texture.get();
    }
  }

  rhi::TextureDesc rhiDesc{.width = desc.width,
                           .height = desc.height,
                           .depth = desc.depth,
                           .mipLevels = desc.mipLevels,
                           .arrayLayers = desc.arrayLayers,
                           .format = desc.format,
                           .usage = desc.usage,
                           .dimension = desc.dimension};

  auto texRes = device.createTexture(rhiDesc);
  if (!texRes) {
    ENGINE_LOG_ERROR(
        "TransientResourcePool: Failed to create transient texture '{}': {}",
        desc.name, texRes.error());
    return nullptr;
  }

  auto &entry = m_textures.emplace_back(PooledTexture{
      .texture = std::move(texRes.value()), .desc = desc, .inUse = true});

  return entry.texture.get();
}

rhi::Buffer *TransientResourcePool::acquireBuffer(rhi::Device &device,
                                                  const RGBufferDesc &desc) {
  for (auto &pooled : m_buffers) {
    if (!pooled.inUse && pooled.desc.size >= desc.size &&
        pooled.desc.usage == desc.usage &&
        pooled.desc.memoryUsage == desc.memoryUsage) {
      pooled.inUse = true;
      return pooled.buffer.get();
    }
  }

  rhi::BufferDesc rhiDesc{
      .size = desc.size, .usage = desc.usage, .memoryUsage = desc.memoryUsage};

  auto bufRes = device.createBuffer(rhiDesc);
  if (!bufRes) {
    ENGINE_LOG_ERROR(
        "TransientResourcePool: Failed to create transient buffer '{}': {}",
        desc.name, bufRes.error());
    return nullptr;
  }

  auto &entry = m_buffers.emplace_back(PooledBuffer{
      .buffer = std::move(bufRes.value()), .desc = desc, .inUse = true});

  return entry.buffer.get();
}

void TransientResourcePool::reset() noexcept {
  for (auto &pooled : m_textures) {
    pooled.inUse = false;
  }
  for (auto &pooled : m_buffers) {
    pooled.inUse = false;
  }
}

} // namespace engine::renderer
