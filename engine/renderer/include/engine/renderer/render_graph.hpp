#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <engine/core/profiler.hpp>
#include <engine/core/types.hpp>
#include <engine/renderer/render_graph_builder.hpp>
#include <engine/renderer/render_graph_types.hpp>
#include <engine/renderer/transient_resource_pool.hpp>
#include <engine/rhi/command_buffer.hpp>
#include <engine/rhi/device.hpp>

namespace engine::renderer {

using RGPassExecuteCallback = std::function<void(rhi::CommandBuffer &cmd)>;
using RGPassSetupCallback = std::function<void(RenderPassBuilder &builder)>;

class RenderGraph {
public:
  RenderGraph();
  explicit RenderGraph(TransientResourcePool &externalPool);
  ~RenderGraph();

  RenderGraph(const RenderGraph &) = delete;
  RenderGraph &operator=(const RenderGraph &) = delete;

  RenderGraph(RenderGraph &&) noexcept = default;
  RenderGraph &operator=(RenderGraph &&) noexcept = default;

  // --- Virtual resource declarations ---
  RGTextureHandle createTexture(const RGTextureDesc &desc);
  RGBufferHandle createBuffer(const RGBufferDesc &desc);

  // --- External resource imports ---
  RGTextureHandle
  importTexture(std::string_view name, rhi::Texture *texture,
                rhi::ImageLayout initialLayout = rhi::ImageLayout::Undefined,
                rhi::ImageLayout finalLayout = rhi::ImageLayout::Undefined);

  RGBufferHandle importBuffer(std::string_view name, rhi::Buffer *buffer);

  // --- Pass declaration ---
  void addPass(std::string_view name, RGPassType type,
               RGPassSetupCallback setup, RGPassExecuteCallback execute);

  // --- Graph compilation & execution ---
  void compile();
  void compile(rhi::Device &device);
  void execute(rhi::Device &device, rhi::CommandBuffer &cmd);

  // --- Frame boundary reset ---
  void reset() noexcept;

  // --- Physical resource queries ---
  [[nodiscard]] rhi::Texture *
  getPhysicalTexture(RGTextureHandle handle) const noexcept;
  [[nodiscard]] rhi::Buffer *
  getPhysicalBuffer(RGBufferHandle handle) const noexcept;

  [[nodiscard]] const RGTextureDesc *
  getTextureDesc(RGTextureHandle handle) const noexcept;
  [[nodiscard]] const RGBufferDesc *
  getBufferDesc(RGBufferHandle handle) const noexcept;

  // --- Internal Builder Callbacks ---
  void addTextureAccess(core::u32 passIndex, const RGTextureAccess &access);
  void addBufferAccess(core::u32 passIndex, const RGBufferAccess &access);
  void addColorAttachment(core::u32 passIndex,
                          const RGColorAttachmentInfo &info);
  void setDepthAttachment(core::u32 passIndex,
                          const RGDepthAttachmentInfo &info);
  void setPassSideEffect(core::u32 passIndex, bool sideEffect) noexcept;

private:
  struct VirtualTexture {
    rhi::Texture *physicalTexture{nullptr};
    RGTextureDesc desc{};
    rhi::ImageLayout initialLayout{rhi::ImageLayout::Undefined};
    rhi::ImageLayout finalLayout{rhi::ImageLayout::Undefined};
    rhi::ImageLayout currentLayout{rhi::ImageLayout::Undefined};
    rhi::PipelineStageFlags currentStage{rhi::PipelineStageFlags::None};
    rhi::AccessFlags currentAccess{rhi::AccessFlags::None};
    core::u32 firstPass{~0u};
    core::u32 lastPass{~0u};
    core::u32 writtenByPass{~0u};
    bool isImported{false};
  };

  struct VirtualBuffer {
    rhi::Buffer *physicalBuffer{nullptr};
    RGBufferDesc desc{};
    rhi::PipelineStageFlags currentStage{rhi::PipelineStageFlags::None};
    rhi::AccessFlags currentAccess{rhi::AccessFlags::None};
    core::u32 firstPass{~0u};
    core::u32 lastPass{~0u};
    core::u32 writtenByPass{~0u};
    bool isImported{false};
  };

  struct PassNode {
    RGPassExecuteCallback executeCallback{};
    std::string name{};
    std::vector<RGTextureAccess> textureAccesses{};
    std::vector<RGBufferAccess> bufferAccesses{};
    std::vector<RGColorAttachmentInfo> colorAttachments{};
    std::vector<rhi::ImageBarrier> imageBarriers{};
    std::vector<rhi::BufferBarrier> bufferBarriers{};
    std::optional<RGDepthAttachmentInfo> depthAttachment{};
    RGPassType type{RGPassType::Graphics};
    bool isSideEffect{false};
    bool isCulled{false};
  };

  TransientResourcePool m_internalPool{};
  TransientResourcePool *m_pool{&m_internalPool};

  std::vector<VirtualTexture> m_textures;
  std::vector<VirtualBuffer> m_buffers;
  std::vector<PassNode> m_passes;
  std::vector<core::u32> m_executionOrder;
  std::vector<rhi::ImageBarrier> m_postImageBarriers;

  core::u32 m_activeTextureCount{0};
  core::u32 m_activeBufferCount{0};
  core::u32 m_activePassCount{0};
  bool m_isCompiled{false};
};

} // namespace engine::renderer
