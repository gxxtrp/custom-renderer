#pragma once

#include <engine/core/types.hpp>
#include <engine/renderer/render_graph_types.hpp>

namespace engine::renderer {

class RenderGraph;

class RenderPassBuilder {
public:
  RenderPassBuilder(RenderGraph &graph, core::u32 passIndex) noexcept;

  RGTextureHandle read(RGTextureHandle handle, rhi::PipelineStageFlags stage,
                       rhi::AccessFlags access, rhi::ImageLayout layout);

  RGTextureHandle write(RGTextureHandle handle, rhi::PipelineStageFlags stage,
                        rhi::AccessFlags access, rhi::ImageLayout layout);

  RGBufferHandle read(RGBufferHandle handle, rhi::PipelineStageFlags stage,
                      rhi::AccessFlags access);

  RGBufferHandle write(RGBufferHandle handle, rhi::PipelineStageFlags stage,
                       rhi::AccessFlags access);

  RGTextureHandle addColorAttachment(RGTextureHandle handle,
                                     rhi::ColorClearValue clearValue = {},
                                     bool clearOnLoad = true);

  RGTextureHandle
  setDepthAttachment(RGTextureHandle handle,
                     rhi::DepthStencilClearValue clearValue = {0.0f, 0},
                     bool clearOnLoad = true);

  void setSideEffect(bool sideEffect = true) noexcept;

private:
  RenderGraph &m_graph;
  core::u32 m_passIndex{0};
};

} // namespace engine::renderer
