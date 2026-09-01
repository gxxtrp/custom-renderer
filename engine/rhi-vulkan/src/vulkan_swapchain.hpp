#pragma once

#include <memory>
#include <vector>

#include <engine/rhi/swapchain.hpp>

#include "vulkan_common.hpp"

namespace engine::rhi_vulkan {

class VulkanDevice;
class VulkanTexture;

struct FrameSyncData {
    VkCommandPool commandPool{VK_NULL_HANDLE};
    VkCommandBuffer commandBuffer{VK_NULL_HANDLE};
    VkSemaphore imageAvailableSemaphore{VK_NULL_HANDLE};
    VkSemaphore renderFinishedSemaphore{VK_NULL_HANDLE};
    VkFence inFlightFence{VK_NULL_HANDLE};
};

class VulkanSwapchain final : public rhi::Swapchain {
public:
    static constexpr core::u32 MAX_FRAMES_IN_FLIGHT = 2;

    static std::expected<std::unique_ptr<VulkanSwapchain>, std::string>
    create(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device, VkQueue presentQueue,
           core::u32 graphicsQueueFamily, const rhi::SwapchainDesc& desc);

    ~VulkanSwapchain() override;

    VulkanSwapchain(const VulkanSwapchain&) = delete;
    VulkanSwapchain& operator=(const VulkanSwapchain&) = delete;

    VulkanSwapchain(VulkanSwapchain&& other) noexcept;
    VulkanSwapchain& operator=(VulkanSwapchain&& other) noexcept;

    std::expected<rhi::AcquireResult, std::string> acquireNextImage(rhi::Semaphore& signalSemaphore,
                                                                    rhi::Fence* signalFence = nullptr) override;
    std::expected<rhi::PresentStatus, std::string> present(core::u32 imageIndex,
                                                           rhi::Semaphore& waitSemaphore) override;
    std::expected<void, std::string> resize(core::u32 width, core::u32 height) override;

    [[nodiscard]] rhi::Texture& getImage(core::u32 index) override;
    [[nodiscard]] core::u32 getImageCount() const noexcept override {
        return static_cast<core::u32>(m_textures.size());
    }
    [[nodiscard]] core::u32 getWidth() const noexcept override { return m_width; }
    [[nodiscard]] core::u32 getHeight() const noexcept override { return m_height; }
    [[nodiscard]] rhi::Format getFormat() const noexcept override { return m_format; }
    [[nodiscard]] rhi::PresentMode getPresentMode() const noexcept override { return m_presentMode; }

    [[nodiscard]] FrameSyncData& getCurrentFrameSync() noexcept { return m_frames[m_currentFrameIndex]; }
    [[nodiscard]] core::u32 getCurrentFrameIndex() const noexcept { return m_currentFrameIndex; }
    void advanceFrameIndex() noexcept { m_currentFrameIndex = (m_currentFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT; }

    [[nodiscard]] VkSurfaceKHR getSurface() const noexcept { return m_surface; }
    [[nodiscard]] VkSwapchainKHR getSwapchain() const noexcept { return m_swapchain; }

private:
    explicit VulkanSwapchain(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device,
                             VkQueue presentQueue, VkSurfaceKHR surface, VkSwapchainKHR swapchain, core::u32 width,
                             core::u32 height, rhi::Format format, rhi::PresentMode presentMode,
                             std::vector<std::unique_ptr<VulkanTexture>> textures, std::vector<FrameSyncData> frames,
                             core::u32 graphicsQueueFamily);

    void cleanupSwapchainOnly();

    VkInstance m_instance{VK_NULL_HANDLE};
    VkPhysicalDevice m_physicalDevice{VK_NULL_HANDLE};
    VkDevice m_device{VK_NULL_HANDLE};
    VkQueue m_presentQueue{VK_NULL_HANDLE};
    VkSurfaceKHR m_surface{VK_NULL_HANDLE};
    VkSwapchainKHR m_swapchain{VK_NULL_HANDLE};

    core::u32 m_width{0};
    core::u32 m_height{0};
    rhi::Format m_format{rhi::Format::BGRA8_UNORM};
    rhi::PresentMode m_presentMode{rhi::PresentMode::Immediate};

    std::vector<std::unique_ptr<VulkanTexture>> m_textures;
    std::vector<FrameSyncData> m_frames;
    core::u32 m_currentFrameIndex{0};
    core::u32 m_graphicsQueueFamily{0};
};

}  // namespace engine::rhi_vulkan
