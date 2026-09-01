#pragma once

#include <engine/rhi/command_buffer.hpp>

#include "vulkan_common.hpp"

namespace engine::rhi_vulkan {

class VulkanCommandBuffer final : public rhi::CommandBuffer {
public:
  VulkanCommandBuffer(
      VkDevice device, VkCommandBuffer cmdBuffer,
      PFN_vkCmdDrawMeshTasksEXT pfnCmdDrawMeshTasks = nullptr,
      PFN_vkCmdDrawMeshTasksIndirectEXT pfnCmdDrawMeshTasksIndirect = nullptr,
      PFN_vkCmdDrawMeshTasksIndirectCountEXT pfnCmdDrawMeshTasksIndirectCount =
          nullptr);
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
  void pushConstants(rhi::PipelineStageFlags stages, core::u32 offset,
                     core::u32 size, const void *data) override;
  void dispatch(core::u32 groupCountX, core::u32 groupCountY,
                core::u32 groupCountZ) override;
  void drawMeshTasks(core::u32 groupCountX, core::u32 groupCountY,
                     core::u32 groupCountZ) override;
  void drawMeshTasksIndirect(rhi::Buffer &buffer, core::usize offset,
                             core::u32 drawCount, core::u32 stride) override;
  void drawMeshTasksIndirectCount(rhi::Buffer &buffer, core::usize offset,
                                  rhi::Buffer &countBuffer,
                                  core::usize countBufferOffset,
                                  core::u32 maxDrawCount,
                                  core::u32 stride) override;
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
  PFN_vkCmdDrawMeshTasksIndirectEXT m_pfnCmdDrawMeshTasksIndirect{nullptr};
  PFN_vkCmdDrawMeshTasksIndirectCountEXT m_pfnCmdDrawMeshTasksIndirectCount{
      nullptr};
  class VulkanPipeline *m_currentPipeline{nullptr};
};

} // namespace engine::rhi_vulkan
