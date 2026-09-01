#include "vulkan_command_buffer.hpp"

#include <utility>
#include <vector>

#include "vulkan_buffer.hpp"
#include "vulkan_descriptor_set.hpp"
#include "vulkan_pipeline.hpp"
#include "vulkan_texture.hpp"

namespace engine::rhi_vulkan {

VulkanCommandBuffer::VulkanCommandBuffer(
    VkDevice device, VkCommandBuffer cmdBuffer,
    PFN_vkCmdDrawMeshTasksEXT pfnCmdDrawMeshTasks,
    PFN_vkCmdDrawMeshTasksIndirectEXT pfnCmdDrawMeshTasksIndirect,
    PFN_vkCmdDrawMeshTasksIndirectCountEXT pfnCmdDrawMeshTasksIndirectCount)
    : m_device(device), m_commandBuffer(cmdBuffer),
      m_pfnCmdDrawMeshTasks(pfnCmdDrawMeshTasks),
      m_pfnCmdDrawMeshTasksIndirect(pfnCmdDrawMeshTasksIndirect),
      m_pfnCmdDrawMeshTasksIndirectCount(pfnCmdDrawMeshTasksIndirectCount) {}

VulkanCommandBuffer::VulkanCommandBuffer(VulkanCommandBuffer &&other) noexcept
    : m_device(std::exchange(other.m_device, VK_NULL_HANDLE)),
      m_commandBuffer(std::exchange(other.m_commandBuffer, VK_NULL_HANDLE)),
      m_pfnCmdDrawMeshTasks(
          std::exchange(other.m_pfnCmdDrawMeshTasks, nullptr)),
      m_pfnCmdDrawMeshTasksIndirect(
          std::exchange(other.m_pfnCmdDrawMeshTasksIndirect, nullptr)),
      m_pfnCmdDrawMeshTasksIndirectCount(
          std::exchange(other.m_pfnCmdDrawMeshTasksIndirectCount, nullptr)) {}

VulkanCommandBuffer &
VulkanCommandBuffer::operator=(VulkanCommandBuffer &&other) noexcept {
  if (this != &other) {
    m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
    m_commandBuffer = std::exchange(other.m_commandBuffer, VK_NULL_HANDLE);
    m_pfnCmdDrawMeshTasks = std::exchange(other.m_pfnCmdDrawMeshTasks, nullptr);
    m_pfnCmdDrawMeshTasksIndirect =
        std::exchange(other.m_pfnCmdDrawMeshTasksIndirect, nullptr);
    m_pfnCmdDrawMeshTasksIndirectCount =
        std::exchange(other.m_pfnCmdDrawMeshTasksIndirectCount, nullptr);
  }
  return *this;
}

void VulkanCommandBuffer::begin() {
  VkCommandBufferBeginInfo beginInfo{};
  beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
  VK_CHECK(vkBeginCommandBuffer(m_commandBuffer, &beginInfo));
}

void VulkanCommandBuffer::end() {
  VK_CHECK(vkEndCommandBuffer(m_commandBuffer));
}

void VulkanCommandBuffer::clearColorTexture(
    rhi::Texture &texture, const rhi::ColorClearValue &clearValue) {
  auto &vkTex = static_cast<VulkanTexture &>(texture);
  VkClearColorValue vkClearColor{};
  vkClearColor.float32[0] = clearValue.r;
  vkClearColor.float32[1] = clearValue.g;
  vkClearColor.float32[2] = clearValue.b;
  vkClearColor.float32[3] = clearValue.a;

  VkImageSubresourceRange range{};
  range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  range.baseMipLevel = 0;
  range.levelCount = vkTex.getMipLevels();
  range.baseArrayLayer = 0;
  range.layerCount = vkTex.getArrayLayers();

  vkCmdClearColorImage(m_commandBuffer, vkTex.getVkImage(),
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &vkClearColor, 1,
                       &range);
}

void VulkanCommandBuffer::pipelineBarrier(
    const rhi::PipelineBarrierDesc &barrier) {
  std::vector<VkMemoryBarrier2> memBarriers;
  memBarriers.reserve(barrier.memoryBarriers.size());
  for (const auto &mb : barrier.memoryBarriers) {
    VkMemoryBarrier2 b{};
    b.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2;
    b.srcStageMask = toVkPipelineStageFlags2(mb.srcStage);
    b.srcAccessMask = toVkAccessFlags2(mb.srcAccess);
    b.dstStageMask = toVkPipelineStageFlags2(mb.dstStage);
    b.dstAccessMask = toVkAccessFlags2(mb.dstAccess);
    memBarriers.push_back(b);
  }

  std::vector<VkBufferMemoryBarrier2> bufBarriers;
  bufBarriers.reserve(barrier.bufferBarriers.size());
  for (const auto &bb : barrier.bufferBarriers) {
    if (bb.buffer == nullptr)
      continue;
    auto &vkBuf = static_cast<VulkanBuffer &>(*bb.buffer);
    VkBufferMemoryBarrier2 b{};
    b.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER_2;
    b.srcStageMask = toVkPipelineStageFlags2(bb.srcStage);
    b.srcAccessMask = toVkAccessFlags2(bb.srcAccess);
    b.dstStageMask = toVkPipelineStageFlags2(bb.dstStage);
    b.dstAccessMask = toVkAccessFlags2(bb.dstAccess);
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.buffer = vkBuf.getVkBuffer();
    b.offset = bb.offset;
    b.size = (bb.size == 0) ? VK_WHOLE_SIZE : bb.size;
    bufBarriers.push_back(b);
  }

  std::vector<VkImageMemoryBarrier2> imgBarriers;
  imgBarriers.reserve(barrier.imageBarriers.size());
  for (const auto &ib : barrier.imageBarriers) {
    if (ib.texture == nullptr)
      continue;
    auto &vkTex = static_cast<VulkanTexture &>(*ib.texture);
    VkImageMemoryBarrier2 b{};
    b.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2;
    b.srcStageMask = toVkPipelineStageFlags2(ib.srcStage);
    b.srcAccessMask = toVkAccessFlags2(ib.srcAccess);
    b.dstStageMask = toVkPipelineStageFlags2(ib.dstStage);
    b.dstAccessMask = toVkAccessFlags2(ib.dstAccess);
    b.oldLayout = toVkImageLayout(ib.oldLayout);
    b.newLayout = toVkImageLayout(ib.newLayout);
    b.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = vkTex.getVkImage();
    b.subresourceRange.aspectMask = toVkImageAspectFlags(vkTex.getFormat());
    b.subresourceRange.baseMipLevel = 0;
    b.subresourceRange.levelCount = vkTex.getMipLevels();
    b.subresourceRange.baseArrayLayer = 0;
    b.subresourceRange.layerCount = vkTex.getArrayLayers();
    imgBarriers.push_back(b);
  }

  VkDependencyInfo depInfo{};
  depInfo.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO;
  depInfo.memoryBarrierCount = static_cast<core::u32>(memBarriers.size());
  depInfo.pMemoryBarriers = memBarriers.data();
  depInfo.bufferMemoryBarrierCount = static_cast<core::u32>(bufBarriers.size());
  depInfo.pBufferMemoryBarriers = bufBarriers.data();
  depInfo.imageMemoryBarrierCount = static_cast<core::u32>(imgBarriers.size());
  depInfo.pImageMemoryBarriers = imgBarriers.data();

  vkCmdPipelineBarrier2(m_commandBuffer, &depInfo);
}

void VulkanCommandBuffer::beginRendering(const rhi::RenderingDesc &desc) {
  std::vector<VkRenderingAttachmentInfo> colorAttachments;
  colorAttachments.reserve(desc.colorAttachments.size());

  for (const auto &att : desc.colorAttachments) {
    if (att.texture == nullptr)
      continue;
    auto &vkTex = static_cast<VulkanTexture &>(*att.texture);
    VkRenderingAttachmentInfo info{};
    info.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    info.imageView = vkTex.getVkImageView();
    info.imageLayout = toVkImageLayout(att.layout);
    info.loadOp = att.clearOnLoad ? VK_ATTACHMENT_LOAD_OP_CLEAR
                                  : VK_ATTACHMENT_LOAD_OP_LOAD;
    info.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    info.clearValue.color.float32[0] = att.clearValue.r;
    info.clearValue.color.float32[1] = att.clearValue.g;
    info.clearValue.color.float32[2] = att.clearValue.b;
    info.clearValue.color.float32[3] = att.clearValue.a;
    colorAttachments.push_back(info);
  }

  VkRenderingInfo renderInfo{};
  renderInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO;
  renderInfo.renderArea.offset.x = desc.renderArea.offset.x;
  renderInfo.renderArea.offset.y = desc.renderArea.offset.y;
  renderInfo.renderArea.extent.width = desc.renderArea.extent.width;
  renderInfo.renderArea.extent.height = desc.renderArea.extent.height;
  renderInfo.layerCount = 1;
  renderInfo.colorAttachmentCount =
      static_cast<core::u32>(colorAttachments.size());
  renderInfo.pColorAttachments = colorAttachments.data();

  VkRenderingAttachmentInfo depthAtt{};
  if (desc.hasDepth && desc.depthAttachment.texture != nullptr) {
    auto &vkTex = static_cast<VulkanTexture &>(*desc.depthAttachment.texture);
    depthAtt.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
    depthAtt.imageView = vkTex.getVkImageView();
    depthAtt.imageLayout = toVkImageLayout(desc.depthAttachment.layout);
    depthAtt.loadOp = desc.depthAttachment.clearOnLoad
                          ? VK_ATTACHMENT_LOAD_OP_CLEAR
                          : VK_ATTACHMENT_LOAD_OP_LOAD;
    depthAtt.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAtt.clearValue.depthStencil.depth =
        desc.depthAttachment.depthStencilClearValue.depth;
    depthAtt.clearValue.depthStencil.stencil =
        desc.depthAttachment.depthStencilClearValue.stencil;
    renderInfo.pDepthAttachment = &depthAtt;
  }

  vkCmdBeginRendering(m_commandBuffer, &renderInfo);
}

void VulkanCommandBuffer::endRendering() { vkCmdEndRendering(m_commandBuffer); }

void VulkanCommandBuffer::setViewport(const rhi::Viewport &viewport) {
  VkViewport vp{};
  vp.x = viewport.x;
  vp.y = viewport.y;
  vp.width = viewport.width;
  vp.height = viewport.height;
  vp.minDepth = viewport.minDepth;
  vp.maxDepth = viewport.maxDepth;
  vkCmdSetViewport(m_commandBuffer, 0, 1, &vp);
}

void VulkanCommandBuffer::setScissor(const rhi::Rect2D &scissor) {
  VkRect2D sc{};
  sc.offset.x = scissor.offset.x;
  sc.offset.y = scissor.offset.y;
  sc.extent.width = scissor.extent.width;
  sc.extent.height = scissor.extent.height;
  vkCmdSetScissor(m_commandBuffer, 0, 1, &sc);
}

void VulkanCommandBuffer::bindPipeline(rhi::Pipeline &pipeline) {
  auto &vkPipeline = static_cast<VulkanPipeline &>(pipeline);
  m_currentPipeline = &vkPipeline;
  const VkPipelineBindPoint bp =
      (pipeline.getBindPoint() == rhi::PipelineBindPoint::Compute)
          ? VK_PIPELINE_BIND_POINT_COMPUTE
          : VK_PIPELINE_BIND_POINT_GRAPHICS;
  vkCmdBindPipeline(m_commandBuffer, bp, vkPipeline.getVkPipeline());
}

void VulkanCommandBuffer::bindDescriptorSet(rhi::DescriptorSet &set,
                                            core::u32 setIndex) {
  auto &vkSet = static_cast<VulkanDescriptorSet &>(set);
  VkDescriptorSet ds = vkSet.getVkDescriptorSet();
  const VkPipelineBindPoint bp =
      (m_currentPipeline &&
       m_currentPipeline->getBindPoint() == rhi::PipelineBindPoint::Compute)
          ? VK_PIPELINE_BIND_POINT_COMPUTE
          : VK_PIPELINE_BIND_POINT_GRAPHICS;
  const VkPipelineLayout layout = m_currentPipeline
                                      ? m_currentPipeline->getVkPipelineLayout()
                                      : vkSet.getVkPipelineLayout();
  vkCmdBindDescriptorSets(m_commandBuffer, bp, layout, setIndex, 1, &ds, 0,
                          nullptr);
}

void VulkanCommandBuffer::pushConstants(rhi::PipelineStageFlags stages,
                                        core::u32 offset, core::u32 size,
                                        const void *data) {
  if (m_currentPipeline == nullptr || data == nullptr || size == 0) {
    return;
  }
  (void)stages;
  vkCmdPushConstants(m_commandBuffer, m_currentPipeline->getVkPipelineLayout(),
                     VK_SHADER_STAGE_ALL, offset, size, data);
}

void VulkanCommandBuffer::dispatch(core::u32 groupCountX, core::u32 groupCountY,
                                   core::u32 groupCountZ) {
  vkCmdDispatch(m_commandBuffer, groupCountX, groupCountY, groupCountZ);
}

void VulkanCommandBuffer::drawMeshTasks(core::u32 groupCountX,
                                        core::u32 groupCountY,
                                        core::u32 groupCountZ) {
  if (m_pfnCmdDrawMeshTasks != nullptr) {
    m_pfnCmdDrawMeshTasks(m_commandBuffer, groupCountX, groupCountY,
                          groupCountZ);
  }
}

void VulkanCommandBuffer::drawMeshTasksIndirect(rhi::Buffer &buffer,
                                                core::usize offset,
                                                core::u32 drawCount,
                                                core::u32 stride) {
  if (m_pfnCmdDrawMeshTasksIndirect != nullptr) {
    auto &vkBuf = static_cast<VulkanBuffer &>(buffer);
    m_pfnCmdDrawMeshTasksIndirect(m_commandBuffer, vkBuf.getVkBuffer(), offset,
                                  drawCount, stride);
  }
}

void VulkanCommandBuffer::drawMeshTasksIndirectCount(
    rhi::Buffer &buffer, core::usize offset, rhi::Buffer &countBuffer,
    core::usize countBufferOffset, core::u32 maxDrawCount, core::u32 stride) {
  if (m_pfnCmdDrawMeshTasksIndirectCount != nullptr) {
    auto &vkBuf = static_cast<VulkanBuffer &>(buffer);
    auto &vkCountBuf = static_cast<VulkanBuffer &>(countBuffer);
    m_pfnCmdDrawMeshTasksIndirectCount(m_commandBuffer, vkBuf.getVkBuffer(),
                                       offset, vkCountBuf.getVkBuffer(),
                                       countBufferOffset, maxDrawCount, stride);
  }
}

void VulkanCommandBuffer::copyBuffer(rhi::Buffer &src, rhi::Buffer &dst,
                                     core::usize size, core::usize srcOffset,
                                     core::usize dstOffset) {
  auto &vkSrc = static_cast<VulkanBuffer &>(src);
  auto &vkDst = static_cast<VulkanBuffer &>(dst);
  VkBufferCopy copyRegion{};
  copyRegion.srcOffset = srcOffset;
  copyRegion.dstOffset = dstOffset;
  copyRegion.size = size;
  vkCmdCopyBuffer(m_commandBuffer, vkSrc.getVkBuffer(), vkDst.getVkBuffer(), 1,
                  &copyRegion);
}

} // namespace engine::rhi_vulkan
