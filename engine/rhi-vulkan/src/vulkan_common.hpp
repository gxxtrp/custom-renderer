#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>

#include <string_view>

#include <engine/core/log.hpp>
#include <engine/core/types.hpp>
#include <engine/rhi/enums.hpp>

namespace engine::rhi_vulkan {

[[nodiscard]] std::string_view vkResultToString(VkResult result) noexcept;

#define VK_CHECK(call)                                                         \
  do {                                                                         \
    const VkResult result_ = (call);                                           \
    if (result_ != VK_SUCCESS) {                                               \
      ENGINE_LOG_ERROR("Vulkan call failed with {}: {} at {}:{}",              \
                       ::engine::rhi_vulkan::vkResultToString(result_), #call, \
                       __FILE__, __LINE__);                                    \
    }                                                                          \
  } while (0)

[[nodiscard]] VkFormat toVkFormat(rhi::Format format) noexcept;
[[nodiscard]] rhi::Format fromVkFormat(VkFormat format) noexcept;
[[nodiscard]] VkImageLayout toVkImageLayout(rhi::ImageLayout layout) noexcept;
[[nodiscard]] VkPipelineStageFlags2
toVkPipelineStageFlags2(rhi::PipelineStageFlags stage) noexcept;
[[nodiscard]] VkAccessFlags2 toVkAccessFlags2(rhi::AccessFlags access) noexcept;
[[nodiscard]] VkPresentModeKHR toVkPresentMode(rhi::PresentMode mode) noexcept;
[[nodiscard]] rhi::PresentMode
fromVkPresentMode(VkPresentModeKHR mode) noexcept;

} // namespace engine::rhi_vulkan
