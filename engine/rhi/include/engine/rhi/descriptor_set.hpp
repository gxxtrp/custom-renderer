#pragma once

#include <span>

#include <engine/core/types.hpp>
#include <engine/rhi/enums.hpp>

namespace engine::rhi {

class Buffer;
class Sampler;
class Texture;

enum class DescriptorType : core::u8 {
  UniformBuffer = 0,
  StorageBuffer,
  SampledTexture,
  StorageTexture,
  Sampler
};

enum class DescriptorBindingFlags : core::u32 {
  None = 0,
  UpdateAfterBind = 1 << 0,
  PartiallyBound = 1 << 1,
  VariableDescriptorCount = 1 << 2,
  UpdateUnusedWhilePending = 1 << 3,
};

[[nodiscard]] constexpr DescriptorBindingFlags
operator|(DescriptorBindingFlags a, DescriptorBindingFlags b) noexcept {
  return static_cast<DescriptorBindingFlags>(static_cast<core::u32>(a) |
                                             static_cast<core::u32>(b));
}

[[nodiscard]] constexpr DescriptorBindingFlags
operator&(DescriptorBindingFlags a, DescriptorBindingFlags b) noexcept {
  return static_cast<DescriptorBindingFlags>(static_cast<core::u32>(a) &
                                             static_cast<core::u32>(b));
}

[[nodiscard]] constexpr DescriptorBindingFlags
operator~(DescriptorBindingFlags a) noexcept {
  return static_cast<DescriptorBindingFlags>(~static_cast<core::u32>(a));
}

constexpr DescriptorBindingFlags &
operator|=(DescriptorBindingFlags &a, DescriptorBindingFlags b) noexcept {
  a = a | b;
  return a;
}

constexpr DescriptorBindingFlags &
operator&=(DescriptorBindingFlags &a, DescriptorBindingFlags b) noexcept {
  a = a & b;
  return a;
}

struct DescriptorBindingDesc {
  PipelineStageFlags stageFlags{PipelineStageFlags::AllGraphics};
  core::u32 binding{0};
  core::u32 count{1};
  DescriptorType type{DescriptorType::UniformBuffer};
  DescriptorBindingFlags flags{DescriptorBindingFlags::None};
};

struct DescriptorSetDesc {
  std::span<const DescriptorBindingDesc> bindings{};
};

struct BufferBindingInfo {
  Buffer *buffer{nullptr};
  core::usize offset{0};
  core::usize range{0};
};

struct TextureBindingInfo {
  Texture *texture{nullptr};
  ImageLayout layout{ImageLayout::ShaderReadOnlyOptimal};
};

struct SamplerBindingInfo {
  const Sampler *sampler{nullptr};
};

class DescriptorSet {
public:
  virtual ~DescriptorSet() = default;

  DescriptorSet(const DescriptorSet &) = delete;
  DescriptorSet &operator=(const DescriptorSet &) = delete;

  DescriptorSet(DescriptorSet &&) noexcept = default;
  DescriptorSet &operator=(DescriptorSet &&) noexcept = default;

  virtual void updateBuffer(core::u32 binding,
                            const BufferBindingInfo &info) = 0;
  virtual void updateBuffers(core::u32 binding, core::u32 firstElement,
                             std::span<const BufferBindingInfo> infos) = 0;
  virtual void updateTexture(core::u32 binding,
                             const TextureBindingInfo &info) = 0;
  virtual void updateTextures(core::u32 binding, core::u32 firstElement,
                              std::span<const TextureBindingInfo> infos) = 0;
  virtual void updateSampler(core::u32 binding,
                             const SamplerBindingInfo &info) = 0;
  virtual void updateSamplers(core::u32 binding, core::u32 firstElement,
                              std::span<const SamplerBindingInfo> infos) = 0;

protected:
  DescriptorSet() = default;
};

} // namespace engine::rhi
