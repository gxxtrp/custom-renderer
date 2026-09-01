#pragma once

#include <span>

#include <engine/core/types.hpp>
#include <engine/rhi/enums.hpp>

namespace engine::rhi {

class Buffer;
class Texture;

enum class DescriptorType : core::u8 { UniformBuffer = 0, StorageBuffer, SampledTexture, StorageTexture, Sampler };

struct DescriptorBindingDesc {
    PipelineStageFlags stageFlags{PipelineStageFlags::AllGraphics};
    core::u32 binding{0};
    core::u32 count{1};
    DescriptorType type{DescriptorType::UniformBuffer};
};

struct DescriptorSetDesc {
    std::span<const DescriptorBindingDesc> bindings{};
};

struct BufferBindingInfo {
    Buffer* buffer{nullptr};
    core::usize offset{0};
    core::usize range{0};
};

struct TextureBindingInfo {
    Texture* texture{nullptr};
    ImageLayout layout{ImageLayout::ShaderReadOnlyOptimal};
};

class DescriptorSet {
public:
    virtual ~DescriptorSet() = default;

    DescriptorSet(const DescriptorSet&) = delete;
    DescriptorSet& operator=(const DescriptorSet&) = delete;

    DescriptorSet(DescriptorSet&&) noexcept = default;
    DescriptorSet& operator=(DescriptorSet&&) noexcept = default;

    virtual void updateBuffer(core::u32 binding, const BufferBindingInfo& info) = 0;
    virtual void updateTexture(core::u32 binding, const TextureBindingInfo& info) = 0;

protected:
    DescriptorSet() = default;
};

}  // namespace engine::rhi
