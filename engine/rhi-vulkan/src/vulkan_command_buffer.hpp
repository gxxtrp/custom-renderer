#pragma once

#include <engine/rhi/command_buffer.hpp>

#include "vulkan_common.hpp"

namespace engine::rhi_vulkan {

class VulkanCommandBuffer final : public rhi::CommandBuffer {
public:
  VulkanCommandBuffer(VkDevice device, VkCommandBuffer cmdBuffer,
                      PFN_vkCmdDrawMeshTasksEXT pfnCmdDrawMeshTasks = nullptr);
  ~VulkanCommandBuffer() override = default;

  VulkanCommandBuffer(const VulkanCommandBuffer &) = delete;
  VulkanCommandBuffer &operator=(const VulkanCommandBuffer &) = delete;

  VulkanCommandBuffer(VulkanCommandBuffer &&other) noexcept;
  VulkanCommandBuffer &operator=(VulkanCommandBuffer &&other) noexcept;

  void begin() override;
  void end() override;

  void clearColorTexture(rhi::Texture &texture,
                         const rhi::ColorClearValue &clearValue) override;
  void pipelineBarrier(const rhi::PipelineBarrierDesc &barrier) override;

  void beginRendering(const rhi::RenderingDesc &desc) override;
  void endRendering() override;

  void setViewport(const rhi::Viewport &viewport) override;
  void setScissor(const rhi::Rect2D &scissor) override;

  void bindPipeline(rhi::Pipeline &pipeline) override;
  void bindDescriptorSet(rhi::DescriptorSet &set, core::u32 setIndex) override;
  void drawMeshTasks(core::u32 groupCountX, core::u32 groupCountY,
                     core::u32 groupCountZ) override;
  void copyBuffer(rhi::Buffer &src, rhi::Buffer &dst, core::usize size,
                  core::usize srcOffset = 0,
                  core::usize dstOffset = 0) override;

  [[nodiscard]] VkCommandBuffer getVkCommandBuffer() const noexcept {
    return m_commandBuffer;
  }

private:
  VkDevice m_device{VK_NULL_HANDLE};
  VkCommandBuffer m_commandBuffer{VK_NULL_HANDLE};
  PFN_vkCmdDrawMeshTasksEXT m_pfnCmdDrawMeshTasks{nullptr};
};

} // namespace engine::rhi_vulkan
