#pragma once

#include <expected>
#include <memory>
#include <string>

#include <engine/rhi/pipeline.hpp>

#include "vulkan_common.hpp"

namespace engine::rhi_vulkan {

class VulkanPipeline final : public rhi::Pipeline {
public:
  static std::expected<std::unique_ptr<VulkanPipeline>, std::string>
  create(VkDevice device, const rhi::PipelineDesc &desc);

  ~VulkanPipeline() override;

  VulkanPipeline(const VulkanPipeline &) = delete;
  VulkanPipeline &operator=(const VulkanPipeline &) = delete;

  VulkanPipeline(VulkanPipeline &&other) noexcept;
  VulkanPipeline &operator=(VulkanPipeline &&other) noexcept;

  [[nodiscard]] rhi::PipelineBindPoint getBindPoint() const noexcept override {
    return m_bindPoint;
  }

  [[nodiscard]] VkPipeline getVkPipeline() const noexcept { return m_pipeline; }
  [[nodiscard]] VkPipelineLayout getVkPipelineLayout() const noexcept {
    return m_pipelineLayout;
  }

private:
  explicit VulkanPipeline(VkDevice device, VkPipeline pipeline,
                          VkPipelineLayout layout,
                          rhi::PipelineBindPoint bindPoint);

  VkDevice m_device{VK_NULL_HANDLE};
  VkPipeline m_pipeline{VK_NULL_HANDLE};
  VkPipelineLayout m_pipelineLayout{VK_NULL_HANDLE};
  rhi::PipelineBindPoint m_bindPoint{rhi::PipelineBindPoint::Graphics};
};

} // namespace engine::rhi_vulkan
