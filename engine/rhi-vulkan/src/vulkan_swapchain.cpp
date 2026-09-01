#include "vulkan_swapchain.hpp"

#include <algorithm>
#include <utility>

#include "vulkan_sync.hpp"
#include "vulkan_texture.hpp"
#include <SDL3/SDL_vulkan.h>

namespace engine::rhi_vulkan {

namespace {

VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats,
                                           rhi::Format preferred) {
    const VkFormat targetVkFormat = toVkFormat(preferred);

    for (const auto& availableFormat : availableFormats) {
        if (availableFormat.format == targetVkFormat &&
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }

    for (const auto& availableFormat : availableFormats) {
        if ((availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM || availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB ||
             availableFormat.format == VK_FORMAT_R8G8B8A8_UNORM || availableFormat.format == VK_FORMAT_R8G8B8A8_SRGB) &&
            availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return availableFormat;
        }
    }

    return availableFormats[0];
}

VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes,
                                       rhi::PresentMode preferred) {
    const VkPresentModeKHR targetVkMode = toVkPresentMode(preferred);

    for (const auto& mode : availablePresentModes) {
        if (mode == targetVkMode) {
            return mode;
        }
    }

    // Per spec: prefer immediate present mode
    for (const auto& mode : availablePresentModes) {
        if (mode == VK_PRESENT_MODE_IMMEDIATE_KHR) {
            return mode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, core::u32 width, core::u32 height) {
    if (capabilities.currentExtent.width != UINT32_MAX) {
        return capabilities.currentExtent;
    }

    VkExtent2D actualExtent = {width, height};
    actualExtent.width =
        std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    actualExtent.height =
        std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

    return actualExtent;
}

}  // namespace

std::expected<std::unique_ptr<VulkanSwapchain>, std::string>
VulkanSwapchain::create(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device, VkQueue presentQueue,
                        core::u32 graphicsQueueFamily, const rhi::SwapchainDesc& desc) {
    if (desc.windowHandle == nullptr) {
        return std::unexpected("Window handle cannot be null when creating Swapchain");
    }

    auto* window = static_cast<SDL_Window*>(desc.windowHandle);
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    if (!SDL_Vulkan_CreateSurface(window, instance, nullptr, &surface)) {
        return std::unexpected(std::string("Failed to create Vulkan window surface: ") + SDL_GetError());
    }

    VkSurfaceCapabilitiesKHR capabilities{};
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities));

    core::u32 formatCount = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
    if (formatCount == 0) {
        SDL_Vulkan_DestroySurface(instance, surface, nullptr);
        return std::unexpected("Failed to find any surface formats on GPU");
    }
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());

    core::u32 presentModeCount = 0;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);
    if (presentModeCount == 0) {
        SDL_Vulkan_DestroySurface(instance, surface, nullptr);
        return std::unexpected("Failed to find any present modes on GPU");
    }
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, presentModes.data());

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(formats, desc.preferredFormat);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(presentModes, desc.preferredPresentMode);
    VkExtent2D extent = chooseSwapExtent(capabilities, desc.width, desc.height);

    core::u32 imageCount = desc.imageCount;
    if (imageCount < capabilities.minImageCount) {
        imageCount = capabilities.minImageCount;
    }
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = surfaceFormat.format;
    createInfo.imageColorSpace = surfaceFormat.colorSpace;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = VK_NULL_HANDLE;

    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkResult res = vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain);
    if (res != VK_SUCCESS) {
        SDL_Vulkan_DestroySurface(instance, surface, nullptr);
        return std::unexpected(std::string("Failed to create swapchain: ") + std::string(vkResultToString(res)));
    }

    core::u32 actualImageCount = 0;
    vkGetSwapchainImagesKHR(device, swapchain, &actualImageCount, nullptr);
    std::vector<VkImage> swapchainImages(actualImageCount);
    vkGetSwapchainImagesKHR(device, swapchain, &actualImageCount, swapchainImages.data());

    const rhi::Format rhiFormat = fromVkFormat(surfaceFormat.format);
    const rhi::TextureDesc texDesc{.width = extent.width,
                                   .height = extent.height,
                                   .depth = 1,
                                   .mipLevels = 1,
                                   .arrayLayers = 1,
                                   .format = rhiFormat,
                                   .usage =
                                       rhi::TextureUsageFlags::ColorAttachment | rhi::TextureUsageFlags::TransferDst,
                                   .dimension = rhi::TextureDimension::Texture2D};

    std::vector<std::unique_ptr<VulkanTexture>> textures;
    textures.reserve(actualImageCount);
    for (auto img : swapchainImages) {
        auto texRes = VulkanTexture::createFromExisting(device, img, texDesc);
        if (!texRes) {
            vkDestroySwapchainKHR(device, swapchain, nullptr);
            SDL_Vulkan_DestroySurface(instance, surface, nullptr);
            return std::unexpected("Failed to create swapchain image view: " + texRes.error());
        }
        textures.push_back(std::move(texRes.value()));
    }

    // Allocate 2 frames in flight synchronization resources
    std::vector<FrameSyncData> frames(MAX_FRAMES_IN_FLIGHT);
    for (core::u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = graphicsQueueFamily;
        VK_CHECK(vkCreateCommandPool(device, &poolInfo, nullptr, &frames[i].commandPool));

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = frames[i].commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;
        VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, &frames[i].commandBuffer));

        VkSemaphoreCreateInfo semInfo{};
        semInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VK_CHECK(vkCreateSemaphore(device, &semInfo, nullptr, &frames[i].imageAvailableSemaphore));
        VK_CHECK(vkCreateSemaphore(device, &semInfo, nullptr, &frames[i].renderFinishedSemaphore));

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        VK_CHECK(vkCreateFence(device, &fenceInfo, nullptr, &frames[i].inFlightFence));
    }

    ENGINE_LOG_INFO("Vulkan swapchain created successfully ({}x{}, Mode: {}, Images: {})", extent.width, extent.height,
                    static_cast<core::u32>(presentMode), actualImageCount);

    return std::unique_ptr<VulkanSwapchain>(new VulkanSwapchain(
        instance, physicalDevice, device, presentQueue, surface, swapchain, extent.width, extent.height, rhiFormat,
        fromVkPresentMode(presentMode), std::move(textures), std::move(frames), graphicsQueueFamily));
}

VulkanSwapchain::VulkanSwapchain(VkInstance instance, VkPhysicalDevice physicalDevice, VkDevice device,
                                 VkQueue presentQueue, VkSurfaceKHR surface, VkSwapchainKHR swapchain, core::u32 width,
                                 core::u32 height, rhi::Format format, rhi::PresentMode presentMode,
                                 std::vector<std::unique_ptr<VulkanTexture>> textures,
                                 std::vector<FrameSyncData> frames, core::u32 graphicsQueueFamily)
    : m_instance(instance), m_physicalDevice(physicalDevice), m_device(device), m_presentQueue(presentQueue),
      m_surface(surface), m_swapchain(swapchain), m_width(width), m_height(height), m_format(format),
      m_presentMode(presentMode), m_textures(std::move(textures)), m_frames(std::move(frames)),
      m_graphicsQueueFamily(graphicsQueueFamily) {}

void VulkanSwapchain::cleanupSwapchainOnly() {
    m_textures.clear();
    if (m_swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
        m_swapchain = VK_NULL_HANDLE;
    }
}

VulkanSwapchain::~VulkanSwapchain() {
    if (m_device != VK_NULL_HANDLE) {
        vkDeviceWaitIdle(m_device);
    }

    cleanupSwapchainOnly();

    for (auto& frame : m_frames) {
        if (frame.inFlightFence != VK_NULL_HANDLE) {
            vkDestroyFence(m_device, frame.inFlightFence, nullptr);
        }
        if (frame.renderFinishedSemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_device, frame.renderFinishedSemaphore, nullptr);
        }
        if (frame.imageAvailableSemaphore != VK_NULL_HANDLE) {
            vkDestroySemaphore(m_device, frame.imageAvailableSemaphore, nullptr);
        }
        if (frame.commandPool != VK_NULL_HANDLE) {
            vkDestroyCommandPool(m_device, frame.commandPool, nullptr);
        }
    }
    m_frames.clear();

    if (m_surface != VK_NULL_HANDLE && m_instance != VK_NULL_HANDLE) {
        SDL_Vulkan_DestroySurface(m_instance, m_surface, nullptr);
        m_surface = VK_NULL_HANDLE;
    }
}

VulkanSwapchain::VulkanSwapchain(VulkanSwapchain&& other) noexcept
    : m_instance(std::exchange(other.m_instance, VK_NULL_HANDLE)),
      m_physicalDevice(std::exchange(other.m_physicalDevice, VK_NULL_HANDLE)),
      m_device(std::exchange(other.m_device, VK_NULL_HANDLE)),
      m_presentQueue(std::exchange(other.m_presentQueue, VK_NULL_HANDLE)),
      m_surface(std::exchange(other.m_surface, VK_NULL_HANDLE)),
      m_swapchain(std::exchange(other.m_swapchain, VK_NULL_HANDLE)), m_width(other.m_width), m_height(other.m_height),
      m_format(other.m_format), m_presentMode(other.m_presentMode), m_textures(std::move(other.m_textures)),
      m_frames(std::move(other.m_frames)), m_currentFrameIndex(other.m_currentFrameIndex),
      m_graphicsQueueFamily(other.m_graphicsQueueFamily) {}

VulkanSwapchain& VulkanSwapchain::operator=(VulkanSwapchain&& other) noexcept {
    if (this != &other) {
        if (m_device != VK_NULL_HANDLE) {
            vkDeviceWaitIdle(m_device);
        }
        cleanupSwapchainOnly();
        for (auto& frame : m_frames) {
            if (frame.inFlightFence != VK_NULL_HANDLE)
                vkDestroyFence(m_device, frame.inFlightFence, nullptr);
            if (frame.renderFinishedSemaphore != VK_NULL_HANDLE)
                vkDestroySemaphore(m_device, frame.renderFinishedSemaphore, nullptr);
            if (frame.imageAvailableSemaphore != VK_NULL_HANDLE)
                vkDestroySemaphore(m_device, frame.imageAvailableSemaphore, nullptr);
            if (frame.commandPool != VK_NULL_HANDLE)
                vkDestroyCommandPool(m_device, frame.commandPool, nullptr);
        }
        if (m_surface != VK_NULL_HANDLE && m_instance != VK_NULL_HANDLE) {
            SDL_Vulkan_DestroySurface(m_instance, m_surface, nullptr);
        }

        m_instance = std::exchange(other.m_instance, VK_NULL_HANDLE);
        m_physicalDevice = std::exchange(other.m_physicalDevice, VK_NULL_HANDLE);
        m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
        m_presentQueue = std::exchange(other.m_presentQueue, VK_NULL_HANDLE);
        m_surface = std::exchange(other.m_surface, VK_NULL_HANDLE);
        m_swapchain = std::exchange(other.m_swapchain, VK_NULL_HANDLE);
        m_width = other.m_width;
        m_height = other.m_height;
        m_format = other.m_format;
        m_presentMode = other.m_presentMode;
        m_textures = std::move(other.m_textures);
        m_frames = std::move(other.m_frames);
        m_currentFrameIndex = other.m_currentFrameIndex;
        m_graphicsQueueFamily = other.m_graphicsQueueFamily;
    }
    return *this;
}

std::expected<rhi::AcquireResult, std::string> VulkanSwapchain::acquireNextImage(rhi::Semaphore& signalSemaphore,
                                                                                 rhi::Fence* signalFence) {
    auto& vkSem = static_cast<VulkanSemaphore&>(signalSemaphore);
    VkFence vkFenceHandle = VK_NULL_HANDLE;
    if (signalFence != nullptr) {
        vkFenceHandle = static_cast<VulkanFence*>(signalFence)->getVkFence();
    }

    core::u32 imageIndex = 0;
    const VkResult res =
        vkAcquireNextImageKHR(m_device, m_swapchain, UINT64_MAX, vkSem.getVkSemaphore(), vkFenceHandle, &imageIndex);

    if (res == VK_SUCCESS) {
        return rhi::AcquireResult{.imageIndex = imageIndex, .status = rhi::AcquireStatus::Success};
    }
    if (res == VK_SUBOPTIMAL_KHR) {
        return rhi::AcquireResult{.imageIndex = imageIndex, .status = rhi::AcquireStatus::Suboptimal};
    }
    if (res == VK_ERROR_OUT_OF_DATE_KHR) {
        return rhi::AcquireResult{.imageIndex = imageIndex, .status = rhi::AcquireStatus::OutOfDate};
    }
    if (res == VK_TIMEOUT || res == VK_NOT_READY) {
        return rhi::AcquireResult{.imageIndex = imageIndex, .status = rhi::AcquireStatus::Timeout};
    }

    return std::unexpected(std::string("vkAcquireNextImageKHR failed: ") + std::string(vkResultToString(res)));
}

std::expected<rhi::PresentStatus, std::string> VulkanSwapchain::present(core::u32 imageIndex,
                                                                        rhi::Semaphore& waitSemaphore) {
    auto& vkSem = static_cast<VulkanSemaphore&>(waitSemaphore);
    VkSemaphore waitSemHandle = vkSem.getVkSemaphore();

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &waitSemHandle;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &m_swapchain;
    presentInfo.pImageIndices = &imageIndex;

    const VkResult res = vkQueuePresentKHR(m_presentQueue, &presentInfo);

    if (res == VK_SUCCESS) {
        return rhi::PresentStatus::Success;
    }
    if (res == VK_SUBOPTIMAL_KHR) {
        return rhi::PresentStatus::Suboptimal;
    }
    if (res == VK_ERROR_OUT_OF_DATE_KHR) {
        return rhi::PresentStatus::OutOfDate;
    }

    return std::unexpected(std::string("vkQueuePresentKHR failed: ") + std::string(vkResultToString(res)));
}

std::expected<void, std::string> VulkanSwapchain::resize(core::u32 width, core::u32 height) {
    if (width == 0 || height == 0) {
        return {};  // Minimized window
    }

    vkDeviceWaitIdle(m_device);

    VkSurfaceCapabilitiesKHR capabilities{};
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_physicalDevice, m_surface, &capabilities));

    VkExtent2D extent = chooseSwapExtent(capabilities, width, height);

    core::u32 imageCount = static_cast<core::u32>(m_textures.size());
    if (imageCount < capabilities.minImageCount) {
        imageCount = capabilities.minImageCount;
    }
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = m_surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = toVkFormat(m_format);
    createInfo.imageColorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;
    createInfo.imageExtent = extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform = capabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = toVkPresentMode(m_presentMode);
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = m_swapchain;

    VkSwapchainKHR newSwapchain = VK_NULL_HANDLE;
    VkResult res = vkCreateSwapchainKHR(m_device, &createInfo, nullptr, &newSwapchain);
    if (res != VK_SUCCESS) {
        return std::unexpected(std::string("Failed to recreate swapchain: ") + std::string(vkResultToString(res)));
    }

    cleanupSwapchainOnly();
    m_swapchain = newSwapchain;
    m_width = extent.width;
    m_height = extent.height;

    core::u32 actualImageCount = 0;
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &actualImageCount, nullptr);
    std::vector<VkImage> swapchainImages(actualImageCount);
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &actualImageCount, swapchainImages.data());

    const rhi::TextureDesc texDesc{.width = m_width,
                                   .height = m_height,
                                   .depth = 1,
                                   .mipLevels = 1,
                                   .arrayLayers = 1,
                                   .format = m_format,
                                   .usage =
                                       rhi::TextureUsageFlags::ColorAttachment | rhi::TextureUsageFlags::TransferDst,
                                   .dimension = rhi::TextureDimension::Texture2D};

    m_textures.reserve(actualImageCount);
    for (auto img : swapchainImages) {
        auto texRes = VulkanTexture::createFromExisting(m_device, img, texDesc);
        if (!texRes) {
            return std::unexpected("Failed to recreate image view: " + texRes.error());
        }
        m_textures.push_back(std::move(texRes.value()));
    }

    ENGINE_LOG_INFO("Vulkan swapchain resized to {}x{}", m_width, m_height);
    return {};
}

rhi::Texture& VulkanSwapchain::getImage(core::u32 index) {
    return *m_textures[index];
}

}  // namespace engine::rhi_vulkan
