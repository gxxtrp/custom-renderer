#pragma once

#include <expected>
#include <memory>
#include <string>

#include <engine/rhi/buffer.hpp>

#include "vulkan_common.hpp"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"
#endif
#if defined(_MSC_VER)
#pragma warning(push, 0)
#endif
#include <vk_mem_alloc.h>
#if defined(_MSC_VER)
#pragma warning(pop)
#endif
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

namespace engine::rhi_vulkan {

class VulkanBuffer final : public rhi::Buffer {
public:
  static std::expected<std::unique_ptr<VulkanBuffer>, std::string>
  create(VkDevice device, VmaAllocator allocator, const rhi::BufferDesc &desc);

  ~VulkanBuffer() override;

  VulkanBuffer(const VulkanBuffer &) = delete;
  VulkanBuffer &operator=(const VulkanBuffer &) = delete;

  VulkanBuffer(VulkanBuffer &&other) noexcept;
  VulkanBuffer &operator=(VulkanBuffer &&other) noexcept;

  [[nodiscard]] core::usize getSize() const noexcept override {
    return m_desc.size;
  }
  [[nodiscard]] rhi::BufferUsageFlags getUsage() const noexcept override {
    return m_desc.usage;
  }
  [[nodiscard]] rhi::MemoryUsage getMemoryUsage() const noexcept override {
    return m_desc.memoryUsage;
  }

  std::expected<void *, std::string> map() override;
  void unmap() override;

  [[nodiscard]] core::u64 getDeviceAddress() const noexcept override;

  [[nodiscard]] VkBuffer getVkBuffer() const noexcept { return m_buffer; }

private:
  explicit VulkanBuffer(VkDevice device, VmaAllocator allocator,
                        VkBuffer buffer, VmaAllocation allocation,
                        rhi::BufferDesc desc);

  VkDevice m_device{VK_NULL_HANDLE};
  VmaAllocator m_allocator{VK_NULL_HANDLE};
  VkBuffer m_buffer{VK_NULL_HANDLE};
  VmaAllocation m_allocation{VK_NULL_HANDLE};
  rhi::BufferDesc m_desc{};
  void *m_mappedPtr{nullptr};
  bool m_isPersistentlyMapped{false};
};

} // namespace engine::rhi_vulkan
