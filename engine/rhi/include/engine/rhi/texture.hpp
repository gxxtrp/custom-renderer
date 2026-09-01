#pragma once

#include <engine/core/types.hpp>
#include <engine/rhi/enums.hpp>

namespace engine::rhi {

struct TextureDesc {
  core::u32 width{1};
  core::u32 height{1};
  core::u32 depth{1};
  core::u32 mipLevels{1};
  core::u32 arrayLayers{1};
  Format format{Format::RGBA8_UNORM};
  TextureUsageFlags usage{TextureUsageFlags::Sampled};
  TextureDimension dimension{TextureDimension::Texture2D};
};

class Texture {
public:
  virtual ~Texture() = default;

  Texture(const Texture &) = delete;
  Texture &operator=(const Texture &) = delete;

  Texture(Texture &&) noexcept = default;
  Texture &operator=(Texture &&) noexcept = default;

  [[nodiscard]] virtual core::u32 getWidth() const noexcept = 0;
  [[nodiscard]] virtual core::u32 getHeight() const noexcept = 0;
  [[nodiscard]] virtual core::u32 getDepth() const noexcept = 0;
  [[nodiscard]] virtual core::u32 getMipLevels() const noexcept = 0;
  [[nodiscard]] virtual core::u32 getArrayLayers() const noexcept = 0;
  [[nodiscard]] virtual Format getFormat() const noexcept = 0;
  [[nodiscard]] virtual TextureUsageFlags getUsage() const noexcept = 0;
  [[nodiscard]] virtual TextureDimension getDimension() const noexcept = 0;

protected:
  Texture() = default;
};

} // namespace engine::rhi
