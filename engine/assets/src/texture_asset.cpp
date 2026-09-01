#include <engine/assets/texture_asset.hpp>

#include <utility>

namespace engine::assets {

TextureAsset::TextureAsset(core::u32 width, core::u32 height,
                           core::u32 channels, rhi::Format format,
                           std::vector<core::u8> data)
    : m_width(width), m_height(height), m_channels(channels), m_format(format),
      m_data(std::move(data)) {}

TextureAsset TextureAsset::create1x1Color(core::u8 r, core::u8 g, core::u8 b,
                                          core::u8 a, rhi::Format format) {
  std::vector<core::u8> data = {r, g, b, a};
  return TextureAsset(1, 1, 4, format, std::move(data));
}

} // namespace engine::assets
