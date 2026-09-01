#include <set>
#include <utility>
#include <vector>

#include <engine/rhi_vulkan/vulkan_device.hpp>

#include "vulkan_buffer.hpp"
#include "vulkan_command_buffer.hpp"
#include "vulkan_descriptor_set.hpp"
#include "vulkan_instance.hpp"
#include "vulkan_physical_device.hpp"
#include "vulkan_pipeline.hpp"
#include "vulkan_profiler.hpp"
#include "vulkan_swapchain.hpp"
#include "vulkan_sync.hpp"
#include "vulkan_texture.hpp"

namespace engine::rhi_vulkan {

struct VulkanDevice::Impl {
  VulkanInstance instance;
  VulkanPhysicalDevice physicalDevice;
  VkDevice device{VK_NULL_HANDLE};
  VmaAllocator allocator{VK_NULL_HANDLE};
  VkQueue graphicsQueue{VK_NULL_HANDLE};
  VkQueue computeQueue{VK_NULL_HANDLE};
  VkQueue transferQueue{VK_NULL_HANDLE};
  VkCommandPool transientCommandPool{VK_NULL_HANDLE};
  PFN_vkCmdDrawMeshTasksEXT pfnCmdDrawMeshTasks{nullptr};
  PFN_vkCmdDrawMeshTasksIndirectEXT pfnCmdDrawMeshTasksIndirect{nullptr};
  PFN_vkCmdDrawMeshTasksIndirectCountEXT pfnCmdDrawMeshTasksIndirectCount{
      nullptr};

  Impl(VulkanInstance inst, VulkanPhysicalDevice physDev, VkDevice dev,
       VmaAllocator alloc, VkQueue gQueue, VkQueue cQueue, VkQueue tQueue,
       VkCommandPool pool, PFN_vkCmdDrawMeshTasksEXT pfn,
       PFN_vkCmdDrawMeshTasksIndirectEXT pfnIndirect,
       PFN_vkCmdDrawMeshTasksIndirectCountEXT pfnIndirectCount)
      : instance(std::move(inst)), physicalDevice(std::move(physDev)),
        device(dev), allocator(alloc), graphicsQueue(gQueue),
        computeQueue(cQueue), transferQueue(tQueue), transientCommandPool(pool),
        pfnCmdDrawMeshTasks(pfn), pfnCmdDrawMeshTasksIndirect(pfnIndirect),
        pfnCmdDrawMeshTasksIndirectCount(pfnIndirectCount) {}

  ~Impl() {
    if (device != VK_NULL_HANDLE) {
      vkDeviceWaitIdle(device);
    }

    VulkanProfiler::get().destroy();

    if (transientCommandPool != VK_NULL_HANDLE && device != VK_NULL_HANDLE) {
      vkDestroyCommandPool(device, transientCommandPool, nullptr);
      transientCommandPool = VK_NULL_HANDLE;
    }

    if (allocator != VK_NULL_HANDLE) {
      vmaDestroyAllocator(allocator);
      allocator = VK_NULL_HANDLE;
    }

    if (device != VK_NULL_HANDLE) {
      vkDestroyDevice(device, nullptr);
      device = VK_NULL_HANDLE;
      ENGINE_LOG_INFO("Vulkan logical device destroyed cleanly");
    }
  }
};

std::expected<std::unique_ptr<VulkanDevice>, std::string>
VulkanDevice::create(const rhi::DeviceDesc &desc) {
  auto instanceRes =
      VulkanInstance::create({.enableValidation = desc.enableValidation});
  if (!instanceRes) {
    return std::unexpected("Failed to initialize Vulkan instance: " +
                           instanceRes.error());
  }
  VulkanInstance instance = std::move(instanceRes.value());

  auto physDeviceRes = VulkanPhysicalDevice::selectBest(instance.getInstance());
  if (!physDeviceRes) {
    return std::unexpected("Failed to select physical device: " +
                           physDeviceRes.error());
  }
  VulkanPhysicalDevice physDevice = std::move(physDeviceRes.value());
  const auto &queueFamilies = physDevice.getQueueFamilies();

  std::set<core::u32> uniqueQueueFamilies = {queueFamilies.graphicsFamily,
                                             queueFamilies.computeFamily,
                                             queueFamilies.transferFamily};

  std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
  const float queuePriority = 1.0f;
  for (core::u32 queueFamily : uniqueQueueFamilies) {
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = queueFamily;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;
    queueCreateInfos.push_back(queueCreateInfo);
  }

  std::vector<const char *> deviceExtensions = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
      VK_EXT_MESH_SHADER_EXTENSION_NAME,
  };

  if (physDevice.supportsRayTracing()) {
    deviceExtensions.push_back("VK_KHR_ray_tracing_pipeline");
    deviceExtensions.push_back("VK_KHR_acceleration_structure");
    deviceExtensions.push_back("VK_KHR_deferred_host_operations");
  }

  VkPhysicalDeviceMeshShaderFeaturesEXT meshShaderFeatures{};
  meshShaderFeatures.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_MESH_SHADER_FEATURES_EXT;
  meshShaderFeatures.meshShader = VK_TRUE;
  meshShaderFeatures.taskShader = VK_TRUE;

  VkPhysicalDeviceVulkan13Features vulkan13Features{};
  vulkan13Features.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
  vulkan13Features.pNext = &meshShaderFeatures;
  vulkan13Features.synchronization2 = VK_TRUE;
  vulkan13Features.dynamicRendering = VK_TRUE;
  vulkan13Features.maintenance4 = VK_TRUE;

  VkPhysicalDeviceVulkan12Features vulkan12Features{};
  vulkan12Features.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
  vulkan12Features.pNext = &vulkan13Features;
  vulkan12Features.descriptorIndexing = VK_TRUE;
  vulkan12Features.bufferDeviceAddress = VK_TRUE;
  vulkan12Features.runtimeDescriptorArray = VK_TRUE;
  vulkan12Features.descriptorBindingVariableDescriptorCount = VK_TRUE;
  vulkan12Features.descriptorBindingPartiallyBound = VK_TRUE;
  vulkan12Features.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
  vulkan12Features.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
  vulkan12Features.descriptorBindingStorageImageUpdateAfterBind = VK_TRUE;
  vulkan12Features.descriptorBindingUpdateUnusedWhilePending = VK_TRUE;
  vulkan12Features.shaderStorageBufferArrayNonUniformIndexing = VK_TRUE;
  vulkan12Features.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;

  VkPhysicalDeviceVulkan11Features vulkan11Features{};
  vulkan11Features.sType =
      VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;
  vulkan11Features.pNext = &vulkan12Features;
  vulkan11Features.shaderDrawParameters = VK_TRUE;

  VkPhysicalDeviceFeatures2 features2{};
  features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
  features2.pNext = &vulkan11Features;
  features2.features.samplerAnisotropy = VK_TRUE;
  features2.features.multiDrawIndirect = VK_TRUE;
  features2.features.drawIndirectFirstInstance = VK_TRUE;
  features2.features.geometryShader = VK_TRUE;

  VkDeviceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  createInfo.pNext = &features2;
  createInfo.queueCreateInfoCount =
      static_cast<core::u32>(queueCreateInfos.size());
  createInfo.pQueueCreateInfos = queueCreateInfos.data();
  createInfo.enabledExtensionCount =
      static_cast<core::u32>(deviceExtensions.size());
  createInfo.ppEnabledExtensionNames = deviceExtensions.data();

  VkDevice device = VK_NULL_HANDLE;
  VkResult result =
      vkCreateDevice(physDevice.getHandle(), &createInfo, nullptr, &device);
  if (result != VK_SUCCESS) {
    return std::unexpected(std::string("Failed to create logical device: ") +
                           std::string(vkResultToString(result)));
  }

  auto pfnCmdDrawMeshTasks = reinterpret_cast<PFN_vkCmdDrawMeshTasksEXT>(
      vkGetDeviceProcAddr(device, "vkCmdDrawMeshTasksEXT"));
  auto pfnCmdDrawMeshTasksIndirect =
      reinterpret_cast<PFN_vkCmdDrawMeshTasksIndirectEXT>(
          vkGetDeviceProcAddr(device, "vkCmdDrawMeshTasksIndirectEXT"));
  auto pfnCmdDrawMeshTasksIndirectCount =
      reinterpret_cast<PFN_vkCmdDrawMeshTasksIndirectCountEXT>(
          vkGetDeviceProcAddr(device, "vkCmdDrawMeshTasksIndirectCountEXT"));

  VkQueue graphicsQueue = VK_NULL_HANDLE;
  VkQueue computeQueue = VK_NULL_HANDLE;
  VkQueue transferQueue = VK_NULL_HANDLE;

  vkGetDeviceQueue(device, queueFamilies.graphicsFamily, 0, &graphicsQueue);
  vkGetDeviceQueue(device, queueFamilies.computeFamily, 0, &computeQueue);
  vkGetDeviceQueue(device, queueFamilies.transferFamily, 0, &transferQueue);

  VmaAllocatorCreateInfo allocatorInfo{};
  allocatorInfo.vulkanApiVersion = VK_API_VERSION_1_3;
  allocatorInfo.physicalDevice = physDevice.getHandle();
  allocatorInfo.device = device;
  allocatorInfo.instance = instance.getInstance();
  allocatorInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;

  VmaAllocator allocator = VK_NULL_HANDLE;
  result = vmaCreateAllocator(&allocatorInfo, &allocator);
  if (result != VK_SUCCESS) {
    vkDestroyDevice(device, nullptr);
    return std::unexpected(std::string("Failed to create VMA allocator: ") +
                           std::string(vkResultToString(result)));
  }

  VkCommandPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  poolInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                   VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
  poolInfo.queueFamilyIndex = queueFamilies.graphicsFamily;

  VkCommandPool transientPool = VK_NULL_HANDLE;
  VK_CHECK(vkCreateCommandPool(device, &poolInfo, nullptr, &transientPool));

  {
    VkCommandBufferAllocateInfo cmdAlloc{};
    cmdAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    cmdAlloc.commandPool = transientPool;
    cmdAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    cmdAlloc.commandBufferCount = 1;
    VkCommandBuffer tracyCmd = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(device, &cmdAlloc, &tracyCmd) == VK_SUCCESS) {
      VulkanProfiler::get().init(physDevice.getHandle(), device, graphicsQueue,
                                 tracyCmd);
      vkFreeCommandBuffers(device, transientPool, 1, &tracyCmd);
    }
  }

  ENGINE_LOG_INFO("Vulkan logical device created successfully");

  auto impl = std::make_unique<Impl>(
      std::move(instance), std::move(physDevice), device, allocator,
      graphicsQueue, computeQueue, transferQueue, transientPool,
      pfnCmdDrawMeshTasks, pfnCmdDrawMeshTasksIndirect,
      pfnCmdDrawMeshTasksIndirectCount);

  return std::unique_ptr<VulkanDevice>(new VulkanDevice(std::move(impl)));
}

VulkanDevice::VulkanDevice(std::unique_ptr<Impl> impl)
    : m_impl(std::move(impl)) {}

VulkanDevice::~VulkanDevice() = default;

VulkanDevice::VulkanDevice(VulkanDevice &&other) noexcept = default;
VulkanDevice &VulkanDevice::operator=(VulkanDevice &&other) noexcept = default;

std::expected<std::unique_ptr<rhi::Buffer>, std::string>
VulkanDevice::createBuffer(const rhi::BufferDesc &desc) {
  auto bufRes = VulkanBuffer::create(m_impl->device, m_impl->allocator, desc);
  if (!bufRes) {
    return std::unexpected(bufRes.error());
  }
  return std::unique_ptr<rhi::Buffer>(std::move(bufRes.value()));
}

std::expected<std::unique_ptr<rhi::Texture>, std::string>
VulkanDevice::createTexture(const rhi::TextureDesc &desc) {
  auto texRes = VulkanTexture::create(m_impl->device, m_impl->allocator, desc);
  if (!texRes) {
    return std::unexpected(texRes.error());
  }
  return std::unique_ptr<rhi::Texture>(std::move(texRes.value()));
}

std::expected<std::unique_ptr<rhi::Pipeline>, std::string>
VulkanDevice::createPipeline(const rhi::PipelineDesc &desc) {
  auto pipeRes = VulkanPipeline::create(m_impl->device, desc);
  if (!pipeRes) {
    return std::unexpected(pipeRes.error());
  }
  return std::unique_ptr<rhi::Pipeline>(std::move(pipeRes.value()));
}

std::expected<std::unique_ptr<rhi::DescriptorSet>, std::string>
VulkanDevice::createDescriptorSet(const rhi::DescriptorSetDesc &desc) {
  auto setRes = VulkanDescriptorSet::create(m_impl->device, desc);
  if (!setRes) {
    return std::unexpected(setRes.error());
  }
  return std::unique_ptr<rhi::DescriptorSet>(std::move(setRes.value()));
}

std::expected<std::unique_ptr<rhi::Fence>, std::string>
VulkanDevice::createFence(const rhi::FenceDesc &desc) {
  return std::make_unique<VulkanFence>(m_impl->device, desc);
}

std::expected<std::unique_ptr<rhi::Semaphore>, std::string>
VulkanDevice::createSemaphore(const rhi::SemaphoreDesc &desc) {
  return std::make_unique<VulkanSemaphore>(m_impl->device, desc);
}

std::expected<std::unique_ptr<rhi::Swapchain>, std::string>
VulkanDevice::createSwapchain(const rhi::SwapchainDesc &desc) {
  auto swapRes = VulkanSwapchain::create(
      m_impl->instance.getInstance(), m_impl->physicalDevice.getHandle(),
      m_impl->device, m_impl->graphicsQueue,
      m_impl->physicalDevice.getQueueFamilies().graphicsFamily, desc);
  if (!swapRes) {
    return std::unexpected(swapRes.error());
  }
  return std::unique_ptr<rhi::Swapchain>(std::move(swapRes.value()));
}

std::expected<std::unique_ptr<rhi::CommandBuffer>, std::string>
VulkanDevice::createCommandBuffer(const rhi::CommandBufferDesc & /*desc*/) {
  VkCommandBufferAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocInfo.commandPool = m_impl->transientCommandPool;
  allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocInfo.commandBufferCount = 1;

  VkCommandBuffer cmd = VK_NULL_HANDLE;
  const VkResult res =
      vkAllocateCommandBuffers(m_impl->device, &allocInfo, &cmd);
  if (res != VK_SUCCESS) {
    return std::unexpected(std::string("Failed to allocate command buffer: ") +
                           std::string(vkResultToString(res)));
  }

  return std::make_unique<VulkanCommandBuffer>(
      m_impl->device, cmd, m_impl->pfnCmdDrawMeshTasks,
      m_impl->pfnCmdDrawMeshTasksIndirect,
      m_impl->pfnCmdDrawMeshTasksIndirectCount);
}

void VulkanDevice::submit(const rhi::SubmitInfo &submitInfo) {
  if (submitInfo.commandBuffer == nullptr) {
    return;
  }

  auto &vkCmd = static_cast<VulkanCommandBuffer &>(*submitInfo.commandBuffer);

  VkCommandBufferSubmitInfo cmdInfo{};
  cmdInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO;
  cmdInfo.commandBuffer = vkCmd.getVkCommandBuffer();

  std::vector<VkSemaphoreSubmitInfo> waitSemaphores;
  waitSemaphores.reserve(submitInfo.waitSemaphores.size());
  for (size_t i = 0; i < submitInfo.waitSemaphores.size(); ++i) {
    if (submitInfo.waitSemaphores[i] == nullptr)
      continue;
    auto &vkSem =
        static_cast<const VulkanSemaphore &>(*submitInfo.waitSemaphores[i]);
    VkSemaphoreSubmitInfo sem{};
    sem.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    sem.semaphore = vkSem.getVkSemaphore();
    sem.stageMask = (i < submitInfo.waitStages.size())
                        ? toVkPipelineStageFlags2(submitInfo.waitStages[i])
                        : VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    waitSemaphores.push_back(sem);
  }

  std::vector<VkSemaphoreSubmitInfo> signalSemaphores;
  signalSemaphores.reserve(submitInfo.signalSemaphores.size());
  for (const auto *s : submitInfo.signalSemaphores) {
    if (s == nullptr)
      continue;
    auto &vkSem = static_cast<const VulkanSemaphore &>(*s);
    VkSemaphoreSubmitInfo sem{};
    sem.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO;
    sem.semaphore = vkSem.getVkSemaphore();
    sem.stageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    signalSemaphores.push_back(sem);
  }

  VkSubmitInfo2 submitInfo2{};
  submitInfo2.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2;
  submitInfo2.commandBufferInfoCount = 1;
  submitInfo2.pCommandBufferInfos = &cmdInfo;
  submitInfo2.waitSemaphoreInfoCount =
      static_cast<core::u32>(waitSemaphores.size());
  submitInfo2.pWaitSemaphoreInfos = waitSemaphores.data();
  submitInfo2.signalSemaphoreInfoCount =
      static_cast<core::u32>(signalSemaphores.size());
  submitInfo2.pSignalSemaphoreInfos = signalSemaphores.data();

  VkFence fenceHandle = VK_NULL_HANDLE;
  if (submitInfo.signalFence != nullptr) {
    fenceHandle =
        static_cast<VulkanFence *>(submitInfo.signalFence)->getVkFence();
  }

  VK_CHECK(vkQueueSubmit2(m_impl->graphicsQueue, 1, &submitInfo2, fenceHandle));
}

void VulkanDevice::waitIdle() {
  if (m_impl && m_impl->device != VK_NULL_HANDLE) {
    vkDeviceWaitIdle(m_impl->device);
  }
}

std::string_view VulkanDevice::getDeviceName() const noexcept {
  return m_impl->physicalDevice.getProperties().deviceName;
}

bool VulkanDevice::isDiscreteGpu() const noexcept {
  return m_impl->physicalDevice.isDiscrete();
}

bool VulkanDevice::supportsRayTracing() const noexcept {
  return m_impl->physicalDevice.supportsRayTracing();
}

bool VulkanDevice::supportsMeshShaders() const noexcept {
  return m_impl->physicalDevice.supportsMeshShaders();
}

} // namespace engine::rhi_vulkan
