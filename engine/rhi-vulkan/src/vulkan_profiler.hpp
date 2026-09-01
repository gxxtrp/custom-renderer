#pragma once

#include "vulkan_common.hpp"

#if defined(TRACY_ENABLE) || defined(ENGINE_PROFILING_ENABLE)
#define TRACY_ENABLE 1
#include <tracy/TracyVulkan.hpp>

namespace engine::rhi_vulkan {

class VulkanProfiler {
public:
  static VulkanProfiler &get() noexcept {
    static VulkanProfiler instance;
    return instance;
  }

  void init(VkPhysicalDevice physicalDevice, VkDevice device, VkQueue queue,
            VkCommandBuffer cmdBuffer) {
    m_ctx = TracyVkContext(physicalDevice, device, queue, cmdBuffer);
  }

  void destroy() {
    if (m_ctx != nullptr) {
      TracyVkDestroy(m_ctx);
      m_ctx = nullptr;
    }
  }

  void collect(VkCommandBuffer cmdBuffer) {
    if (m_ctx != nullptr) {
      TracyVkCollect(m_ctx, cmdBuffer);
    }
  }

  [[nodiscard]] TracyVkCtx getContext() const noexcept { return m_ctx; }

private:
  VulkanProfiler() = default;
  ~VulkanProfiler() { destroy(); }

  TracyVkCtx m_ctx{nullptr};
};

} // namespace engine::rhi_vulkan

#define ENGINE_GPU_ZONE(cmdbuf, name)                                          \
  TracyVkZone(::engine::rhi_vulkan::VulkanProfiler::get().getContext(),        \
              cmdbuf, name)
#define ENGINE_GPU_COLLECT(cmdbuf)                                             \
  ::engine::rhi_vulkan::VulkanProfiler::get().collect(cmdbuf)

#else

namespace engine::rhi_vulkan {

class VulkanProfiler {
public:
  static VulkanProfiler &get() noexcept {
    static VulkanProfiler instance;
    return instance;
  }

  void init(VkPhysicalDevice /*physicalDevice*/, VkDevice /*device*/,
            VkQueue /*queue*/, VkCommandBuffer /*cmdBuffer*/) noexcept {}
  void destroy() noexcept {}
  void collect(VkCommandBuffer /*cmdBuffer*/) noexcept {}
};

} // namespace engine::rhi_vulkan

#define ENGINE_GPU_ZONE(cmdbuf, name)
#define ENGINE_GPU_COLLECT(cmdbuf)

#endif
