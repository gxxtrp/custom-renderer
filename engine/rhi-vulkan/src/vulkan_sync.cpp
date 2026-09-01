#include "vulkan_sync.hpp"

#include <utility>

namespace engine::rhi_vulkan {

// ============================================================================
// VulkanFence
// ============================================================================

VulkanFence::VulkanFence(VkDevice device, const rhi::FenceDesc& desc) : m_device(device) {
    VkFenceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    if (desc.signaled) {
        createInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    }
    VK_CHECK(vkCreateFence(m_device, &createInfo, nullptr, &m_fence));
}

VulkanFence::~VulkanFence() {
    if (m_fence != VK_NULL_HANDLE && m_device != VK_NULL_HANDLE) {
        vkDestroyFence(m_device, m_fence, nullptr);
        m_fence = VK_NULL_HANDLE;
    }
}

VulkanFence::VulkanFence(VulkanFence&& other) noexcept
    : m_device(std::exchange(other.m_device, VK_NULL_HANDLE)), m_fence(std::exchange(other.m_fence, VK_NULL_HANDLE)) {}

VulkanFence& VulkanFence::operator=(VulkanFence&& other) noexcept {
    if (this != &other) {
        if (m_fence != VK_NULL_HANDLE && m_device != VK_NULL_HANDLE) {
            vkDestroyFence(m_device, m_fence, nullptr);
        }
        m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
        m_fence = std::exchange(other.m_fence, VK_NULL_HANDLE);
    }
    return *this;
}

void VulkanFence::wait(core::u64 timeoutNs) {
    if (m_fence != VK_NULL_HANDLE) {
        VK_CHECK(vkWaitForFences(m_device, 1, &m_fence, VK_TRUE, timeoutNs));
    }
}

void VulkanFence::reset() {
    if (m_fence != VK_NULL_HANDLE) {
        VK_CHECK(vkResetFences(m_device, 1, &m_fence));
    }
}

bool VulkanFence::isSignaled() const noexcept {
    if (m_fence == VK_NULL_HANDLE) {
        return false;
    }
    return vkGetFenceStatus(m_device, m_fence) == VK_SUCCESS;
}

// ============================================================================
// VulkanSemaphore
// ============================================================================

VulkanSemaphore::VulkanSemaphore(VkDevice device, const rhi::SemaphoreDesc& desc)
    : m_device(device), m_timeline(desc.timeline) {
    VkSemaphoreTypeCreateInfo typeCreateInfo{};
    typeCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_TYPE_CREATE_INFO;
    typeCreateInfo.semaphoreType = desc.timeline ? VK_SEMAPHORE_TYPE_TIMELINE : VK_SEMAPHORE_TYPE_BINARY;
    typeCreateInfo.initialValue = desc.initialValue;

    VkSemaphoreCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    createInfo.pNext = &typeCreateInfo;

    VK_CHECK(vkCreateSemaphore(m_device, &createInfo, nullptr, &m_semaphore));
}

VulkanSemaphore::~VulkanSemaphore() {
    if (m_semaphore != VK_NULL_HANDLE && m_device != VK_NULL_HANDLE) {
        vkDestroySemaphore(m_device, m_semaphore, nullptr);
        m_semaphore = VK_NULL_HANDLE;
    }
}

VulkanSemaphore::VulkanSemaphore(VulkanSemaphore&& other) noexcept
    : m_device(std::exchange(other.m_device, VK_NULL_HANDLE)),
      m_semaphore(std::exchange(other.m_semaphore, VK_NULL_HANDLE)),
      m_timeline(std::exchange(other.m_timeline, false)) {}

VulkanSemaphore& VulkanSemaphore::operator=(VulkanSemaphore&& other) noexcept {
    if (this != &other) {
        if (m_semaphore != VK_NULL_HANDLE && m_device != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_device, m_semaphore, nullptr);
        }
        m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
        m_semaphore = std::exchange(other.m_semaphore, VK_NULL_HANDLE);
        m_timeline = std::exchange(other.m_timeline, false);
    }
    return *this;
}

core::u64 VulkanSemaphore::getValue() const {
    if (!m_timeline || m_semaphore == VK_NULL_HANDLE) {
        return 0;
    }
    core::u64 val = 0;
    VK_CHECK(vkGetSemaphoreCounterValue(m_device, m_semaphore, &val));
    return val;
}

void VulkanSemaphore::wait(core::u64 value, core::u64 timeoutNs) {
    if (!m_timeline || m_semaphore == VK_NULL_HANDLE) {
        return;
    }
    VkSemaphoreWaitInfo waitInfo{};
    waitInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_WAIT_INFO;
    waitInfo.semaphoreCount = 1;
    waitInfo.pSemaphores = &m_semaphore;
    waitInfo.pValues = &value;
    VK_CHECK(vkWaitSemaphores(m_device, &waitInfo, timeoutNs));
}

void VulkanSemaphore::signal(core::u64 value) {
    if (!m_timeline || m_semaphore == VK_NULL_HANDLE) {
        return;
    }
    VkSemaphoreSignalInfo signalInfo{};
    signalInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_SIGNAL_INFO;
    signalInfo.semaphore = m_semaphore;
    signalInfo.value = value;
    VK_CHECK(vkSignalSemaphore(m_device, &signalInfo));
}

}  // namespace engine::rhi_vulkan
