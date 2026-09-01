#include "vulkan_buffer.hpp"

#include <utility>

namespace engine::rhi_vulkan {

namespace {

VkBufferUsageFlags toVkBufferUsage(rhi::BufferUsageFlags usage) {
    VkBufferUsageFlags flags = 0;
    const auto u = static_cast<core::u32>(usage);
    if ((u & static_cast<core::u32>(rhi::BufferUsageFlags::TransferSrc)) != 0) {
        flags |= VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    }
    if ((u & static_cast<core::u32>(rhi::BufferUsageFlags::TransferDst)) != 0) {
        flags |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }
    if ((u & static_cast<core::u32>(rhi::BufferUsageFlags::UniformBuffer)) != 0) {
        flags |= VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
    }
    if ((u & static_cast<core::u32>(rhi::BufferUsageFlags::StorageBuffer)) != 0) {
        flags |= VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    }
    if ((u & static_cast<core::u32>(rhi::BufferUsageFlags::IndexBuffer)) != 0) {
        flags |= VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
    }
    if ((u & static_cast<core::u32>(rhi::BufferUsageFlags::VertexBuffer)) != 0) {
        flags |= VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
    }
    if ((u & static_cast<core::u32>(rhi::BufferUsageFlags::Indirect)) != 0) {
        flags |= VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
    }
    if ((u & static_cast<core::u32>(rhi::BufferUsageFlags::ShaderDeviceAddress)) != 0) {
        flags |= VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT;
    }
    return flags;
}

VmaMemoryUsage toVmaMemoryUsage(rhi::MemoryUsage usage) {
    switch (usage) {
    case rhi::MemoryUsage::GpuOnly:
        return VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
    case rhi::MemoryUsage::CpuToGpu:
        return VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
    case rhi::MemoryUsage::GpuToCpu:
        return VMA_MEMORY_USAGE_AUTO_PREFER_HOST;
    default:
        return VMA_MEMORY_USAGE_AUTO;
    }
}

}  // namespace

std::expected<std::unique_ptr<VulkanBuffer>, std::string> VulkanBuffer::create(VkDevice device, VmaAllocator allocator,
                                                                               const rhi::BufferDesc& desc) {
    if (desc.size == 0) {
        return std::unexpected("Cannot create buffer with size 0");
    }

    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = desc.size;
    bufferInfo.usage = toVkBufferUsage(desc.usage);
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VmaAllocationCreateInfo allocInfo{};
    allocInfo.usage = toVmaMemoryUsage(desc.memoryUsage);
    if (desc.memoryUsage == rhi::MemoryUsage::CpuToGpu || desc.memoryUsage == rhi::MemoryUsage::GpuToCpu) {
        allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT;
    }

    VkBuffer buffer = VK_NULL_HANDLE;
    VmaAllocation allocation = VK_NULL_HANDLE;
    VmaAllocationInfo allocationInfo{};

    const VkResult res = vmaCreateBuffer(allocator, &bufferInfo, &allocInfo, &buffer, &allocation, &allocationInfo);
    if (res != VK_SUCCESS) {
        return std::unexpected(std::string("Failed to allocate buffer: ") + std::string(vkResultToString(res)));
    }

    auto buf = std::unique_ptr<VulkanBuffer>(new VulkanBuffer(device, allocator, buffer, allocation, desc));
    buf->m_mappedPtr = allocationInfo.pMappedData;
    return buf;
}

VulkanBuffer::VulkanBuffer(VkDevice device, VmaAllocator allocator, VkBuffer buffer, VmaAllocation allocation,
                           rhi::BufferDesc desc)
    : m_device(device), m_allocator(allocator), m_buffer(buffer), m_allocation(allocation), m_desc(desc) {}

VulkanBuffer::~VulkanBuffer() {
    if (m_buffer != VK_NULL_HANDLE && m_allocator != VK_NULL_HANDLE) {
        if (m_mappedPtr != nullptr) {
            vmaUnmapMemory(m_allocator, m_allocation);
            m_mappedPtr = nullptr;
        }
        vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);
        m_buffer = VK_NULL_HANDLE;
        m_allocation = VK_NULL_HANDLE;
    }
}

VulkanBuffer::VulkanBuffer(VulkanBuffer&& other) noexcept
    : m_device(std::exchange(other.m_device, VK_NULL_HANDLE)),
      m_allocator(std::exchange(other.m_allocator, VK_NULL_HANDLE)),
      m_buffer(std::exchange(other.m_buffer, VK_NULL_HANDLE)),
      m_allocation(std::exchange(other.m_allocation, VK_NULL_HANDLE)), m_desc(other.m_desc),
      m_mappedPtr(std::exchange(other.m_mappedPtr, nullptr)) {}

VulkanBuffer& VulkanBuffer::operator=(VulkanBuffer&& other) noexcept {
    if (this != &other) {
        if (m_buffer != VK_NULL_HANDLE && m_allocator != VK_NULL_HANDLE) {
            if (m_mappedPtr != nullptr) {
                vmaUnmapMemory(m_allocator, m_allocation);
            }
            vmaDestroyBuffer(m_allocator, m_buffer, m_allocation);
        }
        m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
        m_allocator = std::exchange(other.m_allocator, VK_NULL_HANDLE);
        m_buffer = std::exchange(other.m_buffer, VK_NULL_HANDLE);
        m_allocation = std::exchange(other.m_allocation, VK_NULL_HANDLE);
        m_desc = other.m_desc;
        m_mappedPtr = std::exchange(other.m_mappedPtr, nullptr);
    }
    return *this;
}

std::expected<void*, std::string> VulkanBuffer::map() {
    if (m_mappedPtr != nullptr) {
        return m_mappedPtr;
    }
    void* data = nullptr;
    const VkResult res = vmaMapMemory(m_allocator, m_allocation, &data);
    if (res != VK_SUCCESS) {
        return std::unexpected(std::string("Failed to map buffer memory: ") + std::string(vkResultToString(res)));
    }
    m_mappedPtr = data;
    return data;
}

void VulkanBuffer::unmap() {
    if (m_mappedPtr != nullptr) {
        vmaUnmapMemory(m_allocator, m_allocation);
        m_mappedPtr = nullptr;
    }
}

core::u64 VulkanBuffer::getDeviceAddress() const noexcept {
    if (m_buffer == VK_NULL_HANDLE || m_device == VK_NULL_HANDLE) {
        return 0;
    }
    VkBufferDeviceAddressInfo info{};
    info.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
    info.buffer = m_buffer;
    return vkGetBufferDeviceAddress(m_device, &info);
}

}  // namespace engine::rhi_vulkan
