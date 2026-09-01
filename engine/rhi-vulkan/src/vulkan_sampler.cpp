#include "vulkan_sampler.hpp"

#include <utility>

namespace engine::rhi_vulkan {

std::expected<std::unique_ptr<VulkanSampler>, std::string>
VulkanSampler::create(VkDevice device, const rhi::SamplerDesc &desc) {
  VkSamplerCreateInfo info{};
  info.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
  info.magFilter = toVkFilter(desc.magFilter);
  info.minFilter = toVkFilter(desc.minFilter);
  info.mipmapMode = toVkSamplerMipmapMode(desc.mipmapMode);
  info.addressModeU = toVkSamplerAddressMode(desc.addressModeU);
  info.addressModeV = toVkSamplerAddressMode(desc.addressModeV);
  info.addressModeW = toVkSamplerAddressMode(desc.addressModeW);
  info.mipLodBias = desc.mipLodBias;
  info.anisotropyEnable = desc.anisotropyEnable ? VK_TRUE : VK_FALSE;
  info.maxAnisotropy = desc.maxAnisotropy;
  info.compareEnable = desc.compareEnable ? VK_TRUE : VK_FALSE;
  info.compareOp = toVkCompareOp(desc.compareOp);
  info.minLod = desc.minLod;
  info.maxLod = desc.maxLod;
  info.borderColor = toVkBorderColor(desc.borderColor);
  info.unnormalizedCoordinates =
      desc.unnormalizedCoordinates ? VK_TRUE : VK_FALSE;

  VkSampler sampler = VK_NULL_HANDLE;
  const VkResult res = vkCreateSampler(device, &info, nullptr, &sampler);
  if (res != VK_SUCCESS) {
    return std::unexpected(std::string("Failed to create Vulkan sampler: ") +
                           std::string(vkResultToString(res)));
  }

  return std::unique_ptr<VulkanSampler>(
      new VulkanSampler(device, sampler, desc));
}

VulkanSampler::VulkanSampler(VkDevice device, VkSampler sampler,
                             rhi::SamplerDesc desc)
    : m_device(device), m_sampler(sampler), m_desc(desc) {}

VulkanSampler::~VulkanSampler() {
  if (m_sampler != VK_NULL_HANDLE && m_device != VK_NULL_HANDLE) {
    vkDestroySampler(m_device, m_sampler, nullptr);
    m_sampler = VK_NULL_HANDLE;
  }
}

VulkanSampler::VulkanSampler(VulkanSampler &&other) noexcept
    : m_device(std::exchange(other.m_device, VK_NULL_HANDLE)),
      m_sampler(std::exchange(other.m_sampler, VK_NULL_HANDLE)),
      m_desc(other.m_desc) {}

VulkanSampler &VulkanSampler::operator=(VulkanSampler &&other) noexcept {
  if (this != &other) {
    if (m_sampler != VK_NULL_HANDLE && m_device != VK_NULL_HANDLE) {
      vkDestroySampler(m_device, m_sampler, nullptr);
    }
    m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
    m_sampler = std::exchange(other.m_sampler, VK_NULL_HANDLE);
    m_desc = other.m_desc;
  }
  return *this;
}

} // namespace engine::rhi_vulkan
