#pragma once

#include <engine/core/types.hpp>

namespace engine::rhi {

enum class Format : core::u32 {
  Undefined = 0,

  // 8-bit formats
  R8_UNORM,
  R8_SNORM,
  R8_UINT,
  R8_SINT,
  RG8_UNORM,
  RG8_SNORM,
  RG8_UINT,
  RG8_SINT,
  RGBA8_UNORM,
  RGBA8_SRGB,
  RGBA8_SNORM,
  RGBA8_UINT,
  RGBA8_SINT,
  BGRA8_UNORM,
  BGRA8_SRGB,

  // 16-bit formats
  R16_UNORM,
  R16_SNORM,
  R16_UINT,
  R16_SINT,
  R16_FLOAT,
  RG16_UNORM,
  RG16_SNORM,
  RG16_UINT,
  RG16_SINT,
  RG16_FLOAT,
  RGBA16_UNORM,
  RGBA16_SNORM,
  RGBA16_UINT,
  RGBA16_SINT,
  RGBA16_FLOAT,

  // 32-bit formats
  R32_UINT,
  R32_SINT,
  R32_FLOAT,
  RG32_UINT,
  RG32_SINT,
  RG32_FLOAT,
  RGB32_UINT,
  RGB32_SINT,
  RGB32_FLOAT,
  RGBA32_UINT,
  RGBA32_SINT,
  RGBA32_FLOAT,

  // Depth / Stencil formats
  D16_UNORM,
  D32_FLOAT,
  D24_UNORM_S8_UINT,
  D32_FLOAT_S8_UINT
};

enum class BufferUsageFlags : core::u32 {
  None = 0,
  TransferSrc = 1 << 0,
  TransferDst = 1 << 1,
  UniformBuffer = 1 << 2,
  StorageBuffer = 1 << 3,
  IndexBuffer = 1 << 4,
  VertexBuffer = 1 << 5,
  Indirect = 1 << 6,
  ShaderDeviceAddress = 1 << 7
};

[[nodiscard]] constexpr BufferUsageFlags
operator|(BufferUsageFlags a, BufferUsageFlags b) noexcept {
  return static_cast<BufferUsageFlags>(static_cast<core::u32>(a) |
                                       static_cast<core::u32>(b));
}

[[nodiscard]] constexpr BufferUsageFlags
operator&(BufferUsageFlags a, BufferUsageFlags b) noexcept {
  return static_cast<BufferUsageFlags>(static_cast<core::u32>(a) &
                                       static_cast<core::u32>(b));
}

[[nodiscard]] constexpr BufferUsageFlags
operator~(BufferUsageFlags a) noexcept {
  return static_cast<BufferUsageFlags>(~static_cast<core::u32>(a));
}

constexpr BufferUsageFlags &operator|=(BufferUsageFlags &a,
                                       BufferUsageFlags b) noexcept {
  a = a | b;
  return a;
}

constexpr BufferUsageFlags &operator&=(BufferUsageFlags &a,
                                       BufferUsageFlags b) noexcept {
  a = a & b;
  return a;
}

enum class TextureUsageFlags : core::u32 {
  None = 0,
  TransferSrc = 1 << 0,
  TransferDst = 1 << 1,
  Sampled = 1 << 2,
  Storage = 1 << 3,
  ColorAttachment = 1 << 4,
  DepthStencilAttachment = 1 << 5,
  TransientAttachment = 1 << 6
};

[[nodiscard]] constexpr TextureUsageFlags
operator|(TextureUsageFlags a, TextureUsageFlags b) noexcept {
  return static_cast<TextureUsageFlags>(static_cast<core::u32>(a) |
                                        static_cast<core::u32>(b));
}

[[nodiscard]] constexpr TextureUsageFlags
operator&(TextureUsageFlags a, TextureUsageFlags b) noexcept {
  return static_cast<TextureUsageFlags>(static_cast<core::u32>(a) &
                                        static_cast<core::u32>(b));
}

[[nodiscard]] constexpr TextureUsageFlags
operator~(TextureUsageFlags a) noexcept {
  return static_cast<TextureUsageFlags>(~static_cast<core::u32>(a));
}

constexpr TextureUsageFlags &operator|=(TextureUsageFlags &a,
                                        TextureUsageFlags b) noexcept {
  a = a | b;
  return a;
}

constexpr TextureUsageFlags &operator&=(TextureUsageFlags &a,
                                        TextureUsageFlags b) noexcept {
  a = a & b;
  return a;
}

enum class PipelineStageFlags : core::u32 {
  None = 0,
  TopOfPipe = 1 << 0,
  DrawIndirect = 1 << 1,
  VertexShader = 1 << 2,
  MeshShader = 1 << 3,
  TaskShader = 1 << 4,
  FragmentShader = 1 << 5,
  EarlyFragmentTests = 1 << 6,
  LateFragmentTests = 1 << 7,
  ColorAttachmentOutput = 1 << 8,
  ComputeShader = 1 << 9,
  Transfer = 1 << 10,
  BottomOfPipe = 1 << 11,
  AllGraphics = 1 << 12,
  AllCommands = 1 << 13
};

[[nodiscard]] constexpr PipelineStageFlags
operator|(PipelineStageFlags a, PipelineStageFlags b) noexcept {
  return static_cast<PipelineStageFlags>(static_cast<core::u32>(a) |
                                         static_cast<core::u32>(b));
}

[[nodiscard]] constexpr PipelineStageFlags
operator&(PipelineStageFlags a, PipelineStageFlags b) noexcept {
  return static_cast<PipelineStageFlags>(static_cast<core::u32>(a) &
                                         static_cast<core::u32>(b));
}

[[nodiscard]] constexpr PipelineStageFlags
operator~(PipelineStageFlags a) noexcept {
  return static_cast<PipelineStageFlags>(~static_cast<core::u32>(a));
}

constexpr PipelineStageFlags &operator|=(PipelineStageFlags &a,
                                         PipelineStageFlags b) noexcept {
  a = a | b;
  return a;
}

constexpr PipelineStageFlags &operator&=(PipelineStageFlags &a,
                                         PipelineStageFlags b) noexcept {
  a = a & b;
  return a;
}

enum class AccessFlags : core::u32 {
  None = 0,
  IndirectCommandRead = 1 << 0,
  IndexRead = 1 << 1,
  VertexAttributeRead = 1 << 2,
  UniformRead = 1 << 3,
  InputAttachmentRead = 1 << 4,
  ShaderRead = 1 << 5,
  ShaderWrite = 1 << 6,
  ColorAttachmentRead = 1 << 7,
  ColorAttachmentWrite = 1 << 8,
  DepthStencilAttachmentRead = 1 << 9,
  DepthStencilAttachmentWrite = 1 << 10,
  TransferRead = 1 << 11,
  TransferWrite = 1 << 12,
  HostRead = 1 << 13,
  HostWrite = 1 << 14,
  MemoryRead = 1 << 15,
  MemoryWrite = 1 << 16
};

[[nodiscard]] constexpr AccessFlags operator|(AccessFlags a,
                                              AccessFlags b) noexcept {
  return static_cast<AccessFlags>(static_cast<core::u32>(a) |
                                  static_cast<core::u32>(b));
}

[[nodiscard]] constexpr AccessFlags operator&(AccessFlags a,
                                              AccessFlags b) noexcept {
  return static_cast<AccessFlags>(static_cast<core::u32>(a) &
                                  static_cast<core::u32>(b));
}

[[nodiscard]] constexpr AccessFlags operator~(AccessFlags a) noexcept {
  return static_cast<AccessFlags>(~static_cast<core::u32>(a));
}

constexpr AccessFlags &operator|=(AccessFlags &a, AccessFlags b) noexcept {
  a = a | b;
  return a;
}

constexpr AccessFlags &operator&=(AccessFlags &a, AccessFlags b) noexcept {
  a = a & b;
  return a;
}

enum class ImageLayout : core::u8 {
  Undefined = 0,
  General,
  ColorAttachmentOptimal,
  DepthStencilAttachmentOptimal,
  DepthStencilReadOnlyOptimal,
  ShaderReadOnlyOptimal,
  TransferSrcOptimal,
  TransferDstOptimal,
  Preinitialized,
  PresentSrc
};

enum class PresentMode : core::u8 { Immediate = 0, Mailbox, Fifo, FifoRelaxed };

enum class MemoryUsage : core::u8 { GpuOnly = 0, CpuToGpu, GpuToCpu };

enum class TextureDimension : core::u8 {
  Texture1D = 0,
  Texture2D,
  Texture3D,
  TextureCube
};

enum class IndexType : core::u8 { Uint16 = 0, Uint32 };

} // namespace engine::rhi
