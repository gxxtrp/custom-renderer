#pragma once

#include <expected>
#include <string>
#include <vector>

#include "vulkan_common.hpp"

namespace engine::rhi_vulkan {

struct VulkanInstanceDesc {
  bool enableValidation{false};
};

class VulkanInstance {
public:
  static std::expected<VulkanInstance, std::string>
  create(const VulkanInstanceDesc &desc);

  ~VulkanInstance();

  VulkanInstance(const VulkanInstance &) = delete;
  VulkanInstance &operator=(const VulkanInstance &) = delete;

  VulkanInstance(VulkanInstance &&other) noexcept;
  VulkanInstance &operator=(VulkanInstance &&other) noexcept;

  [[nodiscard]] VkInstance getInstance() const noexcept { return m_instance; }
  [[nodiscard]] bool isValidationEnabled() const noexcept {
    return m_validationEnabled;
  }

private:
  explicit VulkanInstance(VkInstance instance,
                          VkDebugUtilsMessengerEXT debugMessenger,
                          bool validationEnabled);

  VkInstance m_instance{VK_NULL_HANDLE};
  VkDebugUtilsMessengerEXT m_debugMessenger{VK_NULL_HANDLE};
  bool m_validationEnabled{false};
};

} // namespace engine::rhi_vulkan
