#pragma once

#include <expected>
#include <memory>
#include <string>

#include <engine/rhi/sampler.hpp>

#include "vulkan_common.hpp"

namespace engine::rhi_vulkan {

class VulkanSampler final : public rhi::Sampler {
public:
  static std::expected<std::unique_ptr<VulkanSampler>, std::string>
  create(VkDevice device, const rhi::SamplerDesc &desc);

  ~VulkanSampler() override;

  VulkanSampler(const VulkanSampler &) = delete;
  VulkanSampler &operator=(const VulkanSampler &) = delete;

  VulkanSampler(VulkanSampler &&other) noexcept;
  VulkanSampler &operator=(VulkanSampler &&other) noexcept;

  [[nodiscard]] const rhi::SamplerDesc &getDesc() const noexcept override {
    return m_desc;
  }

  [[nodiscard]] VkSampler getVkSampler() const noexcept { return m_sampler; }

private:
  explicit VulkanSampler(VkDevice device, VkSampler sampler,
                         rhi::SamplerDesc desc);

  VkDevice m_device{VK_NULL_HANDLE};
  VkSampler m_sampler{VK_NULL_HANDLE};
  rhi::SamplerDesc m_desc{};
};

} // namespace engine::rhi_vulkan
