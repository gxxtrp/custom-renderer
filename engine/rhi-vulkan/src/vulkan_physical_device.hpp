#pragma once

#include <expected>
#include <optional>
#include <string>
#include <vector>

#include "vulkan_common.hpp"

namespace engine::rhi_vulkan {

struct QueueFamilyIndices {
  core::u32 graphicsFamily{UINT32_MAX};
  core::u32 computeFamily{UINT32_MAX};
  core::u32 transferFamily{UINT32_MAX};
  core::u32 presentFamily{UINT32_MAX};

  [[nodiscard]] bool isComplete() const noexcept {
    return graphicsFamily != UINT32_MAX && computeFamily != UINT32_MAX &&
           transferFamily != UINT32_MAX;
  }
};

class VulkanPhysicalDevice {
public:
  static std::expected<VulkanPhysicalDevice, std::string>
  selectBest(VkInstance instance, VkSurfaceKHR surface = VK_NULL_HANDLE);

  [[nodiscard]] VkPhysicalDevice getHandle() const noexcept { return m_handle; }
  [[nodiscard]] const QueueFamilyIndices &getQueueFamilies() const noexcept {
    return m_queueFamilies;
  }
  [[nodiscard]] const VkPhysicalDeviceProperties &
  getProperties() const noexcept {
    return m_properties;
  }
  [[nodiscard]] bool isDiscrete() const noexcept {
    return m_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU;
  }
  [[nodiscard]] bool supportsRayTracing() const noexcept {
    return m_supportsRayTracing;
  }
  [[nodiscard]] bool supportsMeshShaders() const noexcept {
    return m_supportsMeshShaders;
  }

  void updatePresentSupport(VkSurfaceKHR surface) noexcept;

private:
  explicit VulkanPhysicalDevice(VkPhysicalDevice handle,
                                QueueFamilyIndices queueFamilies,
                                VkPhysicalDeviceProperties properties,
                                bool supportsRayTracing,
                                bool supportsMeshShaders);

  VkPhysicalDevice m_handle{VK_NULL_HANDLE};
  QueueFamilyIndices m_queueFamilies{};
  VkPhysicalDeviceProperties m_properties{};
  bool m_supportsRayTracing{false};
  bool m_supportsMeshShaders{false};
};

} // namespace engine::rhi_vulkan
