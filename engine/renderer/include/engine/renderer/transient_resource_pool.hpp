#pragma once

#include <memory>
#include <vector>

#include <engine/core/types.hpp>
#include <engine/renderer/render_graph_types.hpp>
#include <engine/rhi/buffer.hpp>
#include <engine/rhi/device.hpp>
#include <engine/rhi/texture.hpp>

namespace engine::renderer {

class TransientResourcePool {
public:
  TransientResourcePool() = default;
  ~TransientResourcePool();

  TransientResourcePool(const TransientResourcePool &) = delete;
  TransientResourcePool &operator=(const TransientResourcePool &) = delete;

  TransientResourcePool(TransientResourcePool &&) noexcept = default;
  TransientResourcePool &operator=(TransientResourcePool &&) noexcept = default;

  [[nodiscard]] rhi::Texture *acquireTexture(rhi::Device &device,
                                             const RGTextureDesc &desc);
  [[nodiscard]] rhi::Buffer *acquireBuffer(rhi::Device &device,
                                           const RGBufferDesc &desc);

  void reset() noexcept;

  [[nodiscard]] core::usize getTextureCount() const noexcept {
    return m_textures.size();
  }
  [[nodiscard]] core::usize getBufferCount() const noexcept {
    return m_buffers.size();
  }

private:
  struct PooledTexture {
    std::unique_ptr<rhi::Texture> texture;
    RGTextureDesc desc{};
    bool inUse{false};
  };

  struct PooledBuffer {
    std::unique_ptr<rhi::Buffer> buffer;
    RGBufferDesc desc{};
    bool inUse{false};
  };

  std::vector<PooledTexture> m_textures;
  std::vector<PooledBuffer> m_buffers;
};

} // namespace engine::renderer
