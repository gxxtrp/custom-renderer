#pragma once

#include <span>

#include <engine/core/types.hpp>
#include <engine/rhi/common.hpp>
#include <engine/rhi/enums.hpp>

namespace engine::rhi {

class Buffer;
class DescriptorSet;
class Pipeline;
class Texture;

struct MemoryBarrier {
  AccessFlags srcAccess{AccessFlags::None};
  AccessFlags dstAccess{AccessFlags::None};
  PipelineStageFlags srcStage{PipelineStageFlags::None};
  PipelineStageFlags dstStage{PipelineStageFlags::None};
};

struct BufferBarrier {
  Buffer *buffer{nullptr};
  core::usize offset{0};
  core::usize size{0};
  AccessFlags srcAccess{AccessFlags::None};
  AccessFlags dstAccess{AccessFlags::None};
  PipelineStageFlags srcStage{PipelineStageFlags::None};
  PipelineStageFlags dstStage{PipelineStageFlags::None};
};

struct ImageBarrier {
  Texture *texture{nullptr};
  AccessFlags srcAccess{AccessFlags::None};
  AccessFlags dstAccess{AccessFlags::None};
  PipelineStageFlags srcStage{PipelineStageFlags::None};
  PipelineStageFlags dstStage{PipelineStageFlags::None};
  ImageLayout oldLayout{ImageLayout::Undefined};
  ImageLayout newLayout{ImageLayout::Undefined};
};

struct PipelineBarrierDesc {
  std::span<const MemoryBarrier> memoryBarriers{};
  std::span<const BufferBarrier> bufferBarriers{};
  std::span<const ImageBarrier> imageBarriers{};
};

struct RenderingAttachmentDesc {
  Texture *texture{nullptr};
  ColorClearValue clearValue{};
  DepthStencilClearValue depthStencilClearValue{.depth = 0.0f, .stencil = 0};
  ImageLayout layout{ImageLayout::ColorAttachmentOptimal};
  bool clearOnLoad{false};
};

struct RenderingDesc {
  std::span<const RenderingAttachmentDesc> colorAttachments{};
  RenderingAttachmentDesc depthAttachment{};
  Rect2D renderArea{};
  bool hasDepth{false};
};

struct CommandBufferDesc {
  bool isSecondary{false};
};

class CommandBuffer {
public:
  virtual ~CommandBuffer() = default;

  CommandBuffer(const CommandBuffer &) = delete;
  CommandBuffer &operator=(const CommandBuffer &) = delete;

  CommandBuffer(CommandBuffer &&) noexcept = default;
  CommandBuffer &operator=(CommandBuffer &&) noexcept = default;

  virtual void begin() = 0;
  virtual void end() = 0;

  virtual void clearColorTexture(Texture &texture,
                                 const ColorClearValue &clearValue) = 0;
  virtual void pipelineBarrier(const PipelineBarrierDesc &barrier) = 0;

  virtual void beginRendering(const RenderingDesc &desc) = 0;
  virtual void endRendering() = 0;

  virtual void setViewport(const Viewport &viewport) = 0;
  virtual void setScissor(const Rect2D &scissor) = 0;

  virtual void bindPipeline(Pipeline &pipeline) = 0;
  virtual void bindDescriptorSet(DescriptorSet &set, core::u32 setIndex) = 0;
  virtual void pushConstants(PipelineStageFlags stages, core::u32 offset,
                             core::u32 size, const void *data) = 0;
  virtual void drawMeshTasks(core::u32 groupCountX, core::u32 groupCountY,
                             core::u32 groupCountZ) = 0;
  virtual void copyBuffer(Buffer &src, Buffer &dst, core::usize size,
                          core::usize srcOffset = 0,
                          core::usize dstOffset = 0) = 0;

protected:
  CommandBuffer() = default;
};

} // namespace engine::rhi
