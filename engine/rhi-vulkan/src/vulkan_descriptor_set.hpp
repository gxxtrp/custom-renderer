#pragma once

#include <expected>
#include <memory>
#include <string>

#include <engine/rhi/descriptor_set.hpp>

#include "vulkan_common.hpp"

namespace engine::rhi_vulkan {

class VulkanDescriptorSet final : public rhi::DescriptorSet {
public:
    static std::expected<std::unique_ptr<VulkanDescriptorSet>, std::string> create(VkDevice device,
                                                                                   const rhi::DescriptorSetDesc& desc);

    ~VulkanDescriptorSet() override;

    VulkanDescriptorSet(const VulkanDescriptorSet&) = delete;
    VulkanDescriptorSet& operator=(const VulkanDescriptorSet&) = delete;

    VulkanDescriptorSet(VulkanDescriptorSet&& other) noexcept;
    VulkanDescriptorSet& operator=(VulkanDescriptorSet&& other) noexcept;

    void updateBuffer(core::u32 binding, const rhi::BufferBindingInfo& info) override;
    void updateTexture(core::u32 binding, const rhi::TextureBindingInfo& info) override;

    [[nodiscard]] VkDescriptorSet getVkDescriptorSet() const noexcept { return m_descriptorSet; }
    [[nodiscard]] VkPipelineLayout getVkPipelineLayout() const noexcept { return m_pipelineLayout; }

private:
    explicit VulkanDescriptorSet(VkDevice device, VkDescriptorPool pool, VkDescriptorSetLayout layout,
                                 VkDescriptorSet set, VkPipelineLayout pipelineLayout);

    VkDevice m_device{VK_NULL_HANDLE};
    VkDescriptorPool m_pool{VK_NULL_HANDLE};
    VkDescriptorSetLayout m_layout{VK_NULL_HANDLE};
    VkDescriptorSet m_descriptorSet{VK_NULL_HANDLE};
    VkPipelineLayout m_pipelineLayout{VK_NULL_HANDLE};
};

}  // namespace engine::rhi_vulkan
