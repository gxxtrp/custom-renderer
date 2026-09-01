#include <engine/renderer/render_graph_builder.hpp>

#include <engine/renderer/render_graph.hpp>

namespace engine::renderer {

RenderPassBuilder::RenderPassBuilder(RenderGraph &graph,
                                     core::u32 passIndex) noexcept
    : m_graph(graph), m_passIndex(passIndex) {}

RGTextureHandle RenderPassBuilder::read(RGTextureHandle handle,
                                        rhi::PipelineStageFlags stage,
                                        rhi::AccessFlags access,
                                        rhi::ImageLayout layout) {
  m_graph.addTextureAccess(m_passIndex, RGTextureAccess{.handle = handle,
                                                        .stage = stage,
                                                        .access = access,
                                                        .layout = layout,
                                                        .isWrite = false});
  return handle;
}

RGTextureHandle RenderPassBuilder::write(RGTextureHandle handle,
                                         rhi::PipelineStageFlags stage,
                                         rhi::AccessFlags access,
                                         rhi::ImageLayout layout) {
  m_graph.addTextureAccess(m_passIndex, RGTextureAccess{.handle = handle,
                                                        .stage = stage,
                                                        .access = access,
                                                        .layout = layout,
                                                        .isWrite = true});
  return handle;
}

RGBufferHandle RenderPassBuilder::read(RGBufferHandle handle,
                                       rhi::PipelineStageFlags stage,
                                       rhi::AccessFlags access) {
  m_graph.addBufferAccess(m_passIndex, RGBufferAccess{.handle = handle,
                                                      .stage = stage,
                                                      .access = access,
                                                      .isWrite = false});
  return handle;
}

RGBufferHandle RenderPassBuilder::write(RGBufferHandle handle,
                                        rhi::PipelineStageFlags stage,
                                        rhi::AccessFlags access) {
  m_graph.addBufferAccess(m_passIndex, RGBufferAccess{.handle = handle,
                                                      .stage = stage,
                                                      .access = access,
                                                      .isWrite = true});
  return handle;
}

RGTextureHandle RenderPassBuilder::addColorAttachment(
    RGTextureHandle handle, rhi::ColorClearValue clearValue, bool clearOnLoad) {
  write(handle, rhi::PipelineStageFlags::ColorAttachmentOutput,
        rhi::AccessFlags::ColorAttachmentWrite,
        rhi::ImageLayout::ColorAttachmentOptimal);
  m_graph.addColorAttachment(m_passIndex,
                             RGColorAttachmentInfo{.handle = handle,
                                                   .clearValue = clearValue,
                                                   .clearOnLoad = clearOnLoad});
  return handle;
}

RGTextureHandle
RenderPassBuilder::setDepthAttachment(RGTextureHandle handle,
                                      rhi::DepthStencilClearValue clearValue,
                                      bool clearOnLoad) {
  write(handle,
        rhi::PipelineStageFlags::EarlyFragmentTests |
            rhi::PipelineStageFlags::LateFragmentTests,
        rhi::AccessFlags::DepthStencilAttachmentRead |
            rhi::AccessFlags::DepthStencilAttachmentWrite,
        rhi::ImageLayout::DepthStencilAttachmentOptimal);
  m_graph.setDepthAttachment(m_passIndex,
                             RGDepthAttachmentInfo{.handle = handle,
                                                   .clearValue = clearValue,
                                                   .clearOnLoad = clearOnLoad});
  return handle;
}

void RenderPassBuilder::setSideEffect(bool sideEffect) noexcept {
  m_graph.setPassSideEffect(m_passIndex, sideEffect);
}

} // namespace engine::renderer
