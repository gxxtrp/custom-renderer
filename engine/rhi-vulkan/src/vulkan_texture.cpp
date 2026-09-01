#include "vulkan_texture.hpp"

#include <utility>

namespace engine::rhi_vulkan {

namespace {

VkImageUsageFlags toVkImageUsage(rhi::TextureUsageFlags usage) {
  VkImageUsageFlags flags = 0;
  const auto u = static_cast<core::u32>(usage);
  if ((u & static_cast<core::u32>(rhi::TextureUsageFlags::TransferSrc)) != 0) {
    flags |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
  }
  if ((u & static_cast<core::u32>(rhi::TextureUsageFlags::TransferDst)) != 0) {
    flags |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  }
  if ((u & static_cast<core::u32>(rhi::TextureUsageFlags::Sampled)) != 0) {
    flags |= VK_IMAGE_USAGE_SAMPLED_BIT;
  }
  if ((u & static_cast<core::u32>(rhi::TextureUsageFlags::Storage)) != 0) {
    flags |= VK_IMAGE_USAGE_STORAGE_BIT;
  }
  if ((u & static_cast<core::u32>(rhi::TextureUsageFlags::ColorAttachment)) !=
      0) {
    flags |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
  }
  if ((u & static_cast<core::u32>(
               rhi::TextureUsageFlags::DepthStencilAttachment)) != 0) {
    flags |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
  }
  if ((u & static_cast<core::u32>(
               rhi::TextureUsageFlags::TransientAttachment)) != 0) {
    flags |= VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT;
  }
  return flags;
}

VkImageType toVkImageType(rhi::TextureDimension dimension) {
  switch (dimension) {
  case rhi::TextureDimension::Texture1D:
    return VK_IMAGE_TYPE_1D;
  case rhi::TextureDimension::Texture2D:
    return VK_IMAGE_TYPE_2D;
  case rhi::TextureDimension::Texture3D:
    return VK_IMAGE_TYPE_3D;
  case rhi::TextureDimension::TextureCube:
    return VK_IMAGE_TYPE_2D;
  default:
    return VK_IMAGE_TYPE_2D;
  }
}

VkImageViewType toVkImageViewType(rhi::TextureDimension dimension,
                                  core::u32 arrayLayers) {
  switch (dimension) {
  case rhi::TextureDimension::Texture1D:
    return (arrayLayers > 1) ? VK_IMAGE_VIEW_TYPE_1D_ARRAY
                             : VK_IMAGE_VIEW_TYPE_1D;
  case rhi::TextureDimension::Texture2D:
    return (arrayLayers > 1) ? VK_IMAGE_VIEW_TYPE_2D_ARRAY
                             : VK_IMAGE_VIEW_TYPE_2D;
  case rhi::TextureDimension::Texture3D:
    return VK_IMAGE_VIEW_TYPE_3D;
  case rhi::TextureDimension::TextureCube:
    return (arrayLayers > 6) ? VK_IMAGE_VIEW_TYPE_CUBE_ARRAY
                             : VK_IMAGE_VIEW_TYPE_CUBE;
  default:
    return VK_IMAGE_VIEW_TYPE_2D;
  }
}

} // namespace

std::expected<std::unique_ptr<VulkanTexture>, std::string>
VulkanTexture::create(VkDevice device, VmaAllocator allocator,
                      const rhi::TextureDesc &desc) {
  VkImageCreateInfo imageInfo{};
  imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageInfo.imageType = toVkImageType(desc.dimension);
  imageInfo.extent.width = desc.width;
  imageInfo.extent.height = desc.height;
  imageInfo.extent.depth = desc.depth;
  imageInfo.mipLevels = desc.mipLevels;
  imageInfo.arrayLayers = desc.arrayLayers;
  imageInfo.format = toVkFormat(desc.format);
  imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
  imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageInfo.usage = toVkImageUsage(desc.usage);
  imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  if (desc.dimension == rhi::TextureDimension::TextureCube) {
    imageInfo.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
  }

  VmaAllocationCreateInfo allocInfo{};
  allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;

  VkImage image = VK_NULL_HANDLE;
  VmaAllocation allocation = VK_NULL_HANDLE;
  const VkResult res = vmaCreateImage(allocator, &imageInfo, &allocInfo, &image,
                                      &allocation, nullptr);
  if (res != VK_SUCCESS) {
    return std::unexpected(std::string("Failed to allocate image via VMA: ") +
                           std::string(vkResultToString(res)));
  }

  VkImageViewCreateInfo viewInfo{};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = image;
  viewInfo.viewType = toVkImageViewType(desc.dimension, desc.arrayLayers);
  viewInfo.format = imageInfo.format;
  viewInfo.subresourceRange.aspectMask = toVkImageAspectFlags(desc.format);
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = desc.mipLevels;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = desc.arrayLayers;

  VkImageView imageView = VK_NULL_HANDLE;
  const VkResult viewRes =
      vkCreateImageView(device, &viewInfo, nullptr, &imageView);
  if (viewRes != VK_SUCCESS) {
    vmaDestroyImage(allocator, image, allocation);
    return std::unexpected(std::string("Failed to create image view: ") +
                           std::string(vkResultToString(viewRes)));
  }

  return std::unique_ptr<VulkanTexture>(new VulkanTexture(
      device, allocator, image, imageView, allocation, desc, true));
}

std::expected<std::unique_ptr<VulkanTexture>, std::string>
VulkanTexture::createFromExisting(VkDevice device, VkImage image,
                                  const rhi::TextureDesc &desc) {
  VkImageViewCreateInfo viewInfo{};
  viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  viewInfo.image = image;
  viewInfo.viewType = toVkImageViewType(desc.dimension, desc.arrayLayers);
  viewInfo.format = toVkFormat(desc.format);
  viewInfo.subresourceRange.aspectMask = toVkImageAspectFlags(desc.format);
  viewInfo.subresourceRange.baseMipLevel = 0;
  viewInfo.subresourceRange.levelCount = desc.mipLevels;
  viewInfo.subresourceRange.baseArrayLayer = 0;
  viewInfo.subresourceRange.layerCount = desc.arrayLayers;

  VkImageView imageView = VK_NULL_HANDLE;
  const VkResult viewRes =
      vkCreateImageView(device, &viewInfo, nullptr, &imageView);
  if (viewRes != VK_SUCCESS) {
    return std::unexpected(
        std::string("Failed to create image view for existing image: ") +
        std::string(vkResultToString(viewRes)));
  }

  return std::unique_ptr<VulkanTexture>(new VulkanTexture(
      device, VK_NULL_HANDLE, image, imageView, VK_NULL_HANDLE, desc, false));
}

VulkanTexture::VulkanTexture(VkDevice device, VmaAllocator allocator,
                             VkImage image, VkImageView imageView,
                             VmaAllocation allocation, rhi::TextureDesc desc,
                             bool ownsImage)
    : m_device(device), m_allocator(allocator), m_image(image),
      m_imageView(imageView), m_allocation(allocation), m_desc(desc),
      m_ownsImage(ownsImage) {}

VulkanTexture::~VulkanTexture() {
  if (m_imageView != VK_NULL_HANDLE && m_device != VK_NULL_HANDLE) {
    vkDestroyImageView(m_device, m_imageView, nullptr);
    m_imageView = VK_NULL_HANDLE;
  }
  if (m_ownsImage && m_image != VK_NULL_HANDLE &&
      m_allocator != VK_NULL_HANDLE) {
    vmaDestroyImage(m_allocator, m_image, m_allocation);
    m_image = VK_NULL_HANDLE;
    m_allocation = VK_NULL_HANDLE;
  }
}

VulkanTexture::VulkanTexture(VulkanTexture &&other) noexcept
    : m_device(std::exchange(other.m_device, VK_NULL_HANDLE)),
      m_allocator(std::exchange(other.m_allocator, VK_NULL_HANDLE)),
      m_image(std::exchange(other.m_image, VK_NULL_HANDLE)),
      m_imageView(std::exchange(other.m_imageView, VK_NULL_HANDLE)),
      m_allocation(std::exchange(other.m_allocation, VK_NULL_HANDLE)),
      m_desc(other.m_desc),
      m_ownsImage(std::exchange(other.m_ownsImage, false)) {}

VulkanTexture &VulkanTexture::operator=(VulkanTexture &&other) noexcept {
  if (this != &other) {
    if (m_imageView != VK_NULL_HANDLE && m_device != VK_NULL_HANDLE) {
      vkDestroyImageView(m_device, m_imageView, nullptr);
    }
    if (m_ownsImage && m_image != VK_NULL_HANDLE &&
        m_allocator != VK_NULL_HANDLE) {
      vmaDestroyImage(m_allocator, m_image, m_allocation);
    }
    m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
    m_allocator = std::exchange(other.m_allocator, VK_NULL_HANDLE);
    m_image = std::exchange(other.m_image, VK_NULL_HANDLE);
    m_imageView = std::exchange(other.m_imageView, VK_NULL_HANDLE);
    m_allocation = std::exchange(other.m_allocation, VK_NULL_HANDLE);
    m_desc = other.m_desc;
    m_ownsImage = std::exchange(other.m_ownsImage, false);
  }
  return *this;
}

} // namespace engine::rhi_vulkan
