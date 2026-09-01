#pragma once

#include <engine/rhi/sync.hpp>

#include "vulkan_common.hpp"

namespace engine::rhi_vulkan {

class VulkanFence final : public rhi::Fence {
public:
    VulkanFence(VkDevice device, const rhi::FenceDesc& desc);
    ~VulkanFence() override;

    VulkanFence(const VulkanFence&) = delete;
    VulkanFence& operator=(const VulkanFence&) = delete;

    VulkanFence(VulkanFence&& other) noexcept;
    VulkanFence& operator=(VulkanFence&& other) noexcept;

    void wait(core::u64 timeoutNs = UINT64_MAX) override;
    void reset() override;
    [[nodiscard]] bool isSignaled() const noexcept override;

    [[nodiscard]] VkFence getVkFence() const noexcept { return m_fence; }

private:
    VkDevice m_device{VK_NULL_HANDLE};
    VkFence m_fence{VK_NULL_HANDLE};
};

class VulkanSemaphore final : public rhi::Semaphore {
public:
    VulkanSemaphore(VkDevice device, const rhi::SemaphoreDesc& desc);
    ~VulkanSemaphore() override;

    VulkanSemaphore(const VulkanSemaphore&) = delete;
    VulkanSemaphore& operator=(const VulkanSemaphore&) = delete;

    VulkanSemaphore(VulkanSemaphore&& other) noexcept;
    VulkanSemaphore& operator=(VulkanSemaphore&& other) noexcept;

    [[nodiscard]] bool isTimeline() const noexcept override { return m_timeline; }
    [[nodiscard]] core::u64 getValue() const override;
    void wait(core::u64 value, core::u64 timeoutNs = UINT64_MAX) override;
    void signal(core::u64 value) override;

    [[nodiscard]] VkSemaphore getVkSemaphore() const noexcept { return m_semaphore; }

private:
    VkDevice m_device{VK_NULL_HANDLE};
    VkSemaphore m_semaphore{VK_NULL_HANDLE};
    bool m_timeline{false};
};

}  // namespace engine::rhi_vulkan
