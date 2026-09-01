#pragma once

#include <span>
#include <vector>

#include <engine/core/types.hpp>
#include <engine/rhi/enums.hpp>

namespace engine::assets {

class TextureAsset {
public:
  TextureAsset() = default;
  ~TextureAsset() = default;

  TextureAsset(const TextureAsset &) = default;
  TextureAsset &operator=(const TextureAsset &) = default;

  TextureAsset(TextureAsset &&) noexcept = default;
  TextureAsset &operator=(TextureAsset &&) noexcept = default;

  TextureAsset(core::u32 width, core::u32 height, core::u32 channels,
               rhi::Format format, std::vector<core::u8> data);

  [[nodiscard]] core::u32 getWidth() const noexcept { return m_width; }
  [[nodiscard]] core::u32 getHeight() const noexcept { return m_height; }
  [[nodiscard]] core::u32 getChannels() const noexcept { return m_channels; }
  [[nodiscard]] rhi::Format getFormat() const noexcept { return m_format; }
  [[nodiscard]] std::span<const core::u8> getData() const noexcept {
    return m_data;
  }
  [[nodiscard]] core::usize getDataSize() const noexcept {
    return m_data.size();
  }
  [[nodiscard]] bool isValid() const noexcept {
    return m_width > 0 && m_height > 0 && !m_data.empty();
  }

  static TextureAsset
  create1x1Color(core::u8 r, core::u8 g, core::u8 b, core::u8 a = 255,
                 rhi::Format format = rhi::Format::RGBA8_UNORM);

private:
  core::u32 m_width{0};
  core::u32 m_height{0};
  core::u32 m_channels{4};
  rhi::Format m_format{rhi::Format::RGBA8_UNORM};
  std::vector<core::u8> m_data;
};

} // namespace engine::assets
