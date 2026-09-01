#include "vulkan_physical_device.hpp"

#include <algorithm>
#include <cstring>

namespace engine::rhi_vulkan {

namespace {

QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device,
                                     VkSurfaceKHR surface) {
  QueueFamilyIndices indices{};

  core::u32 queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);
  std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount,
                                           queueFamilies.data());

  // 1. Find Graphics Queue Family
  for (core::u32 i = 0; i < queueFamilyCount; ++i) {
    if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      indices.graphicsFamily = i;
      break;
    }
  }

  // 2. Find Dedicated Compute Queue Family (prefers non-graphics)
  for (core::u32 i = 0; i < queueFamilyCount; ++i) {
    if ((queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) &&
        !(queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) {
      indices.computeFamily = i;
      break;
    }
  }
  // Fallback compute to graphics family
  if (indices.computeFamily == UINT32_MAX) {
    indices.computeFamily = indices.graphicsFamily;
  }

  // 3. Find Dedicated Transfer Queue Family (prefers non-graphics and
  // non-compute)
  for (core::u32 i = 0; i < queueFamilyCount; ++i) {
    if ((queueFamilies[i].queueFlags & VK_QUEUE_TRANSFER_BIT) &&
        !(queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) &&
        !(queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT)) {
      indices.transferFamily = i;
      break;
    }
  }
  // Fallback transfer to compute or graphics family
  if (indices.transferFamily == UINT32_MAX) {
    indices.transferFamily = indices.graphicsFamily;
  }

  // 4. Find Presentation Support if surface provided
  if (surface != VK_NULL_HANDLE) {
    for (core::u32 i = 0; i < queueFamilyCount; ++i) {
      VkBool32 presentSupport = VK_FALSE;
      vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);
      if (presentSupport == VK_TRUE) {
        indices.presentFamily = i;
        break;
      }
    }
  } else {
    indices.presentFamily = indices.graphicsFamily;
  }

  return indices;
}

bool checkExtensionSupport(VkPhysicalDevice device, const char *extensionName) {
  core::u32 count = 0;
  vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr);
  std::vector<VkExtensionProperties> extensions(count);
  vkEnumerateDeviceExtensionProperties(device, nullptr, &count,
                                       extensions.data());

  for (const auto &ext : extensions) {
    if (std::strcmp(ext.extensionName, extensionName) == 0) {
      return true;
    }
  }
  return false;
}

core::u32 rateDeviceSuitability(VkPhysicalDevice device, VkSurfaceKHR surface,
                                bool &outSupportsRayTracing,
                                bool &outSupportsMeshShaders) {
  outSupportsRayTracing = false;
  outSupportsMeshShaders = false;

  VkPhysicalDeviceProperties properties;
  vkGetPhysicalDeviceProperties(device, &properties);

  // Require Vulkan 1.3
  if (properties.apiVersion < VK_API_VERSION_1_3) {
    ENGINE_LOG_WARN("Device '{}' rejected: API version {}.{}.{} < 1.3",
                    properties.deviceName,
                    VK_VERSION_MAJOR(properties.apiVersion),
                    VK_VERSION_MINOR(properties.apiVersion),
                    VK_VERSION_PATCH(properties.apiVersion));
    return 0;
  }

  // Require swapchain support
  if (!checkExtensionSupport(device, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) {
    ENGINE_LOG_WARN("Device '{}' rejected: Missing VK_KHR_swapchain",
                    properties.deviceName);
    return 0;
  }

  // Require mesh shader extension support
  if (!checkExtensionSupport(device, VK_EXT_MESH_SHADER_EXTENSION_NAME)) {
    ENGINE_LOG_WARN("Device '{}' rejected: Missing VK_EXT_mesh_shader",
                    properties.deviceName);
    return 0;
  }

  // Query mesh shader features
  VkPhysicalDeviceMeshShaderFeaturesEXT meshFeatures{};
  meshFeatures.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;
  VkPhysicalDeviceFeatures2 features2{};
  features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  features2.pNext = &meshFeatures;
  vkGetPhysicalDeviceFeatures2(device, &features2);

  if (meshFeatures.meshShader != VK_TRUE) {
    ENGINE_LOG_WARN("Device '{}' rejected: meshShader feature not enabled",
                    properties.deviceName);
    return 0;
  }
  outSupportsMeshShaders = true;

  // Check queue families
  QueueFamilyIndices indices = findQueueFamilies(device, surface);
  if (!indices.isComplete()) {
    ENGINE_LOG_WARN("Device '{}' rejected: Incomplete queue families",
                    properties.deviceName);
    return 0;
  }

  // Check ray tracing support
  const bool rtPipeline =
      checkExtensionSupport(device, "VK_KHR_ray_tracing_pipeline");
  const bool asSupport =
      checkExtensionSupport(device, "VK_KHR_acceleration_structure");
  outSupportsRayTracing = rtPipeline && asSupport;

  core::u32 score = 100;

  // Discrete GPU heavily favored
  if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
    score += 10000;
  }

  // Check VRAM size
  VkPhysicalDeviceMemoryProperties memProperties;
  vkGetPhysicalDeviceMemoryProperties(device, &memProperties);
  core::u64 vramBytes = 0;
  for (core::u32 i = 0; i < memProperties.memoryHeapCount; ++i) {
    if (memProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
      vramBytes += memProperties.memoryHeaps[i].size;
    }
  }
  score += static_cast<core::u32>(vramBytes / (1024ULL * 1024ULL));

  // Ray tracing bonus
  if (outSupportsRayTracing) {
    score += 1000;
  }

  ENGINE_LOG_INFO("Device '{}' (Type: {}) scored {}", properties.deviceName,
                  static_cast<core::u32>(properties.deviceType), score);
  return score;
}

} // namespace

std::expected<VulkanPhysicalDevice, std::string>
VulkanPhysicalDevice::selectBest(VkInstance instance, VkSurfaceKHR surface) {
  core::u32 deviceCount = 0;
  vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
  if (deviceCount == 0) {
    return std::unexpected("Failed to find any GPUs with Vulkan support");
  }

  std::vector<VkPhysicalDevice> devices(deviceCount);
  vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

  VkPhysicalDevice bestDevice = VK_NULL_HANDLE;
  core::u32 highestScore = 0;
  bool bestRt = false;
  bool bestMesh = false;

  for (const auto &dev : devices) {
    bool rt = false;
    bool mesh = false;
    const core::u32 score = rateDeviceSuitability(dev, surface, rt, mesh);
    if (score > highestScore) {
      highestScore = score;
      bestDevice = dev;
      bestRt = rt;
      bestMesh = mesh;
    }
  }

  if (bestDevice == VK_NULL_HANDLE || highestScore == 0) {
    return std::unexpected(
        "No suitable Vulkan 1.3 GPU with VK_EXT_mesh_shader support found");
  }

  VkPhysicalDeviceProperties props;
  vkGetPhysicalDeviceProperties(bestDevice, &props);
  QueueFamilyIndices queueFamilies = findQueueFamilies(bestDevice, surface);

  ENGINE_LOG_INFO(
      "Selected GPU: '{}' (Discrete: {}, MeshShaders: {}, RayTracing: {})",
      props.deviceName,
      props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU, bestMesh,
      bestRt);
  ENGINE_LOG_INFO(
      "Queue Families: Graphics={}, Compute={}, Transfer={}, Present={}",
      queueFamilies.graphicsFamily, queueFamilies.computeFamily,
      queueFamilies.transferFamily, queueFamilies.presentFamily);

  return VulkanPhysicalDevice(bestDevice, queueFamilies, props, bestRt,
                              bestMesh);
}

VulkanPhysicalDevice::VulkanPhysicalDevice(
    VkPhysicalDevice handle, QueueFamilyIndices queueFamilies,
    VkPhysicalDeviceProperties properties, bool supportsRayTracing,
    bool supportsMeshShaders)
    : m_handle(handle), m_queueFamilies(queueFamilies),
      m_properties(properties), m_supportsRayTracing(supportsRayTracing),
      m_supportsMeshShaders(supportsMeshShaders) {}

void VulkanPhysicalDevice::updatePresentSupport(VkSurfaceKHR surface) noexcept {
  if (surface == VK_NULL_HANDLE) {
    return;
  }
  core::u32 queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(m_handle, &queueFamilyCount,
                                           nullptr);
  for (core::u32 i = 0; i < queueFamilyCount; ++i) {
    VkBool32 presentSupport = VK_FALSE;
    vkGetPhysicalDeviceSurfaceSupportKHR(m_handle, i, surface, &presentSupport);
    if (presentSupport == VK_TRUE) {
      m_queueFamilies.presentFamily = i;
      break;
    }
  }
}

} // namespace engine::rhi_vulkan
