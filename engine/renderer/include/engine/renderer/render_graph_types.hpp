#pragma once

#include <compare>
#include <string>
#include <string_view>

#include <engine/core/types.hpp>
#include <engine/rhi/command_buffer.hpp>
#include <engine/rhi/common.hpp>
#include <engine/rhi/enums.hpp>

namespace engine::renderer {

struct RGTextureHandle {
  core::u32 id{core::u32(~0u)};

  [[nodiscard]] constexpr bool isValid() const noexcept {
    return id != core::u32(~0u);
  }
  [[nodiscard]] constexpr auto
  operator<=>(const RGTextureHandle &) const = default;

  static constexpr RGTextureHandle invalid() noexcept {
    return RGTextureHandle{core::u32(~0u)};
  }
};

struct RGBufferHandle {
  core::u32 id{core::u32(~0u)};

  [[nodiscard]] constexpr bool isValid() const noexcept {
    return id != core::u32(~0u);
  }
  [[nodiscard]] constexpr auto
  operator<=>(const RGBufferHandle &) const = default;

  static constexpr RGBufferHandle invalid() noexcept {
    return RGBufferHandle{core::u32(~0u)};
  }
};

enum class RGPassType : core::u8 { Graphics = 0, Compute, Transfer };

struct RGTextureDesc {
  std::string name{};
  core::u32 width{0};
  core::u32 height{0};
  core::u32 depth{1};
  core::u32 mipLevels{1};
  core::u32 arrayLayers{1};
  rhi::Format format{rhi::Format::RGBA8_UNORM};
  rhi::TextureUsageFlags usage{rhi::TextureUsageFlags::ColorAttachment};
  rhi::TextureDimension dimension{rhi::TextureDimension::Texture2D};
};

struct RGBufferDesc {
  std::string name{};
  core::usize size{0};
  rhi::BufferUsageFlags usage{rhi::BufferUsageFlags::StorageBuffer};
  rhi::MemoryUsage memoryUsage{rhi::MemoryUsage::GpuOnly};
};

struct RGColorAttachmentInfo {
  RGTextureHandle handle{RGTextureHandle::invalid()};
  rhi::ColorClearValue clearValue{};
  bool clearOnLoad{true};
};

struct RGDepthAttachmentInfo {
  RGTextureHandle handle{RGTextureHandle::invalid()};
  rhi::DepthStencilClearValue clearValue{0.0f, 0};
  bool clearOnLoad{true};
};

struct RGTextureAccess {
  RGTextureHandle handle{RGTextureHandle::invalid()};
  rhi::PipelineStageFlags stage{rhi::PipelineStageFlags::None};
  rhi::AccessFlags access{rhi::AccessFlags::None};
  rhi::ImageLayout layout{rhi::ImageLayout::Undefined};
  bool isWrite{false};
};

struct RGBufferAccess {
  RGBufferHandle handle{RGBufferHandle::invalid()};
  rhi::PipelineStageFlags stage{rhi::PipelineStageFlags::None};
  rhi::AccessFlags access{rhi::AccessFlags::None};
  bool isWrite{false};
};

} // namespace engine::renderer
