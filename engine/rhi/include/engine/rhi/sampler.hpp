#pragma once

#include <engine/core/types.hpp>
#include <engine/rhi/enums.hpp>

namespace engine::rhi {

struct SamplerDesc {
  Filter minFilter{Filter::Linear};
  Filter magFilter{Filter::Linear};
  SamplerMipmapMode mipmapMode{SamplerMipmapMode::Linear};
  SamplerAddressMode addressModeU{SamplerAddressMode::Repeat};
  SamplerAddressMode addressModeV{SamplerAddressMode::Repeat};
  SamplerAddressMode addressModeW{SamplerAddressMode::Repeat};
  core::f32 mipLodBias{0.0f};
  bool anisotropyEnable{true};
  core::f32 maxAnisotropy{16.0f};
  bool compareEnable{false};
  CompareOp compareOp{CompareOp::Always};
  core::f32 minLod{0.0f};
  core::f32 maxLod{1000.0f};
  BorderColor borderColor{BorderColor::FloatOpaqueBlack};
  bool unnormalizedCoordinates{false};
};

class Sampler {
public:
  virtual ~Sampler() = default;

  Sampler(const Sampler &) = delete;
  Sampler &operator=(const Sampler &) = delete;

  Sampler(Sampler &&) noexcept = default;
  Sampler &operator=(Sampler &&) noexcept = default;

  [[nodiscard]] virtual const SamplerDesc &getDesc() const noexcept = 0;

protected:
  Sampler() = default;
};

} // namespace engine::rhi
