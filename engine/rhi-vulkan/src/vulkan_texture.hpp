#pragma once

#include <expected>
#include <memory>
#include <string>

#include <engine/rhi/texture.hpp>

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

class VulkanTexture final : public rhi::Texture {
public:
  static std::expected<std::unique_ptr<VulkanTexture>, std::string>
  create(VkDevice device, VmaAllocator allocator, const rhi::TextureDesc &desc);

  static std::expected<std::unique_ptr<VulkanTexture>, std::string>
  createFromExisting(VkDevice device, VkImage image,
                     const rhi::TextureDesc &desc);

  ~VulkanTexture() override;

  VulkanTexture(const VulkanTexture &) = delete;
  VulkanTexture &operator=(const VulkanTexture &) = delete;

  VulkanTexture(VulkanTexture &&other) noexcept;
  VulkanTexture &operator=(VulkanTexture &&other) noexcept;

  [[nodiscard]] core::u32 getWidth() const noexcept override {
    return m_desc.width;
  }
  [[nodiscard]] core::u32 getHeight() const noexcept override {
    return m_desc.height;
  }
  [[nodiscard]] core::u32 getDepth() const noexcept override {
    return m_desc.depth;
  }
  [[nodiscard]] core::u32 getMipLevels() const noexcept override {
    return m_desc.mipLevels;
  }
  [[nodiscard]] core::u32 getArrayLayers() const noexcept override {
    return m_desc.arrayLayers;
  }
  [[nodiscard]] rhi::Format getFormat() const noexcept override {
    return m_desc.format;
  }
  [[nodiscard]] rhi::TextureUsageFlags getUsage() const noexcept override {
    return m_desc.usage;
  }
  [[nodiscard]] rhi::TextureDimension getDimension() const noexcept override {
    return m_desc.dimension;
  }

  [[nodiscard]] VkImage getVkImage() const noexcept { return m_image; }
  [[nodiscard]] VkImageView getVkImageView() const noexcept {
    return m_imageView;
  }

private:
  explicit VulkanTexture(VkDevice device, VmaAllocator allocator, VkImage image,
                         VkImageView imageView, VmaAllocation allocation,
                         rhi::TextureDesc desc, bool ownsImage);

  VkDevice m_device{VK_NULL_HANDLE};
  VmaAllocator m_allocator{VK_NULL_HANDLE};
  VkImage m_image{VK_NULL_HANDLE};
  VkImageView m_imageView{VK_NULL_HANDLE};
  VmaAllocation m_allocation{VK_NULL_HANDLE};
  rhi::TextureDesc m_desc{};
  bool m_ownsImage{false};
};

} // namespace engine::rhi_vulkan
