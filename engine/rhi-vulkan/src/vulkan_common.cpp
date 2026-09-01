#include "vulkan_common.hpp"

namespace engine::rhi_vulkan {

std::string_view vkResultToString(VkResult result) noexcept {
  switch (result) {
  case VK_SUCCESS:
    return "VK_SUCCESS";
  case VK_NOT_READY:
    return "VK_NOT_READY";
  case VK_TIMEOUT:
    return "VK_TIMEOUT";
  case VK_EVENT_SET:
    return "VK_EVENT_SET";
  case VK_EVENT_RESET:
    return "VK_EVENT_RESET";
  case VK_INCOMPLETE:
    return "VK_INCOMPLETE";
  case VK_ERROR_OUT_OF_HOST_MEMORY:
    return "VK_ERROR_OUT_OF_HOST_MEMORY";
  case VK_ERROR_OUT_OF_DEVICE_MEMORY:
    return "VK_ERROR_OUT_OF_DEVICE_MEMORY";
  case VK_ERROR_INITIALIZATION_FAILED:
    return "VK_ERROR_INITIALIZATION_FAILED";
  case VK_ERROR_DEVICE_LOST:
    return "VK_ERROR_DEVICE_LOST";
  case VK_ERROR_MEMORY_MAP_FAILED:
    return "VK_ERROR_MEMORY_MAP_FAILED";
  case VK_ERROR_LAYER_NOT_PRESENT:
    return "VK_ERROR_LAYER_NOT_PRESENT";
  case VK_ERROR_EXTENSION_NOT_PRESENT:
    return "VK_ERROR_EXTENSION_NOT_PRESENT";
  case VK_ERROR_FEATURE_NOT_PRESENT:
    return "VK_ERROR_FEATURE_NOT_PRESENT";
  case VK_ERROR_INCOMPATIBLE_DRIVER:
    return "VK_ERROR_INCOMPATIBLE_DRIVER";
  case VK_ERROR_TOO_MANY_OBJECTS:
    return "VK_ERROR_TOO_MANY_OBJECTS";
  case VK_ERROR_FORMAT_NOT_SUPPORTED:
    return "VK_ERROR_FORMAT_NOT_SUPPORTED";
  case VK_ERROR_FRAGMENTED_POOL:
    return "VK_ERROR_FRAGMENTED_POOL";
  case VK_ERROR_UNKNOWN:
    return "VK_ERROR_UNKNOWN";
  case VK_ERROR_OUT_OF_DATE_KHR:
    return "VK_ERROR_OUT_OF_DATE_KHR";
  case VK_SUBOPTIMAL_KHR:
    return "VK_SUBOPTIMAL_KHR";
  case VK_ERROR_SURFACE_LOST_KHR:
    return "VK_ERROR_SURFACE_LOST_KHR";
  case VK_ERROR_NATIVE_WINDOW_IN_USE_KHR:
    return "VK_ERROR_NATIVE_WINDOW_IN_USE_KHR";
  default:
    return "VK_UNKNOWN_RESULT";
  }
}

VkFormat toVkFormat(rhi::Format format) noexcept {
  switch (format) {
  case rhi::Format::R8_UNORM:
    return VK_FORMAT_R8_UNORM;
  case rhi::Format::R8_SNORM:
    return VK_FORMAT_R8_SNORM;
  case rhi::Format::R8_UINT:
    return VK_FORMAT_R8_UINT;
  case rhi::Format::R8_SINT:
    return VK_FORMAT_R8_SINT;
  case rhi::Format::RG8_UNORM:
    return VK_FORMAT_R8G8_UNORM;
  case rhi::Format::RG8_SNORM:
    return VK_FORMAT_R8G8_SNORM;
  case rhi::Format::RG8_UINT:
    return VK_FORMAT_R8G8_UINT;
  case rhi::Format::RG8_SINT:
    return VK_FORMAT_R8G8_SINT;
  case rhi::Format::RGBA8_UNORM:
    return VK_FORMAT_R8G8B8A8_UNORM;
  case rhi::Format::RGBA8_SRGB:
    return VK_FORMAT_R8G8B8A8_SRGB;
  case rhi::Format::RGBA8_SNORM:
    return VK_FORMAT_R8G8B8A8_SNORM;
  case rhi::Format::RGBA8_UINT:
    return VK_FORMAT_R8G8B8A8_UINT;
  case rhi::Format::RGBA8_SINT:
    return VK_FORMAT_R8G8B8A8_SINT;
  case rhi::Format::BGRA8_UNORM:
    return VK_FORMAT_B8G8R8A8_UNORM;
  case rhi::Format::BGRA8_SRGB:
    return VK_FORMAT_B8G8R8A8_SRGB;
  case rhi::Format::R16_UNORM:
    return VK_FORMAT_R16_UNORM;
  case rhi::Format::R16_SNORM:
    return VK_FORMAT_R16_SNORM;
  case rhi::Format::R16_UINT:
    return VK_FORMAT_R16_UINT;
  case rhi::Format::R16_SINT:
    return VK_FORMAT_R16_SINT;
  case rhi::Format::R16_FLOAT:
    return VK_FORMAT_R16_SFLOAT;
  case rhi::Format::RG16_UNORM:
    return VK_FORMAT_R16G16_UNORM;
  case rhi::Format::RG16_SNORM:
    return VK_FORMAT_R16G16_SNORM;
  case rhi::Format::RG16_UINT:
    return VK_FORMAT_R16G16_UINT;
  case rhi::Format::RG16_SINT:
    return VK_FORMAT_R16G16_SINT;
  case rhi::Format::RG16_FLOAT:
    return VK_FORMAT_R16G16_SFLOAT;
  case rhi::Format::RGBA16_UNORM:
    return VK_FORMAT_R16G16B16A16_UNORM;
  case rhi::Format::RGBA16_SNORM:
    return VK_FORMAT_R16G16B16A16_SNORM;
  case rhi::Format::RGBA16_UINT:
    return VK_FORMAT_R16G16B16A16_UINT;
  case rhi::Format::RGBA16_SINT:
    return VK_FORMAT_R16G16B16A16_SINT;
  case rhi::Format::RGBA16_FLOAT:
    return VK_FORMAT_R16G16B16A16_SFLOAT;
  case rhi::Format::R32_UINT:
    return VK_FORMAT_R32_UINT;
  case rhi::Format::R32_SINT:
    return VK_FORMAT_R32_SINT;
  case rhi::Format::R32_FLOAT:
    return VK_FORMAT_R32_SFLOAT;
  case rhi::Format::RG32_UINT:
    return VK_FORMAT_R32G32_UINT;
  case rhi::Format::RG32_SINT:
    return VK_FORMAT_R32G32_SINT;
  case rhi::Format::RG32_FLOAT:
    return VK_FORMAT_R32G32_SFLOAT;
  case rhi::Format::RGB32_UINT:
    return VK_FORMAT_R32G32B32_UINT;
  case rhi::Format::RGB32_SINT:
    return VK_FORMAT_R32G32B32_SINT;
  case rhi::Format::RGB32_FLOAT:
    return VK_FORMAT_R32G32B32_SFLOAT;
  case rhi::Format::RGBA32_UINT:
    return VK_FORMAT_R32G32B32A32_UINT;
  case rhi::Format::RGBA32_SINT:
    return VK_FORMAT_R32G32B32A32_SINT;
  case rhi::Format::RGBA32_FLOAT:
    return VK_FORMAT_R32G32B32A32_SFLOAT;
  case rhi::Format::D16_UNORM:
    return VK_FORMAT_D16_UNORM;
  case rhi::Format::D32_FLOAT:
    return VK_FORMAT_D32_SFLOAT;
  case rhi::Format::D24_UNORM_S8_UINT:
    return VK_FORMAT_D24_UNORM_S8_UINT;
  case rhi::Format::D32_FLOAT_S8_UINT:
    return VK_FORMAT_D32_SFLOAT_S8_UINT;
  case rhi::Format::Undefined:
  default:
    return VK_FORMAT_UNDEFINED;
  }
}

rhi::Format fromVkFormat(VkFormat format) noexcept {
  switch (format) {
  case VK_FORMAT_R8_UNORM:
    return rhi::Format::R8_UNORM;
  case VK_FORMAT_R8_SNORM:
    return rhi::Format::R8_SNORM;
  case VK_FORMAT_R8_UINT:
    return rhi::Format::R8_UINT;
  case VK_FORMAT_R8_SINT:
    return rhi::Format::R8_SINT;
  case VK_FORMAT_R8G8_UNORM:
    return rhi::Format::RG8_UNORM;
  case VK_FORMAT_R8G8_SNORM:
    return rhi::Format::RG8_SNORM;
  case VK_FORMAT_R8G8_UINT:
    return rhi::Format::RG8_UINT;
  case VK_FORMAT_R8G8_SINT:
    return rhi::Format::RG8_SINT;
  case VK_FORMAT_R8G8B8A8_UNORM:
    return rhi::Format::RGBA8_UNORM;
  case VK_FORMAT_R8G8B8A8_SRGB:
    return rhi::Format::RGBA8_SRGB;
  case VK_FORMAT_R8G8B8A8_SNORM:
    return rhi::Format::RGBA8_SNORM;
  case VK_FORMAT_R8G8B8A8_UINT:
    return rhi::Format::RGBA8_UINT;
  case VK_FORMAT_R8G8B8A8_SINT:
    return rhi::Format::RGBA8_SINT;
  case VK_FORMAT_B8G8R8A8_UNORM:
    return rhi::Format::BGRA8_UNORM;
  case VK_FORMAT_B8G8R8A8_SRGB:
    return rhi::Format::BGRA8_SRGB;
  case VK_FORMAT_R16_UNORM:
    return rhi::Format::R16_UNORM;
  case VK_FORMAT_R16_SNORM:
    return rhi::Format::R16_SNORM;
  case VK_FORMAT_R16_UINT:
    return rhi::Format::R16_UINT;
  case VK_FORMAT_R16_SINT:
    return rhi::Format::R16_SINT;
  case VK_FORMAT_R16_SFLOAT:
    return rhi::Format::R16_FLOAT;
  case VK_FORMAT_R16G16_UNORM:
    return rhi::Format::RG16_UNORM;
  case VK_FORMAT_R16G16_SNORM:
    return rhi::Format::RG16_SNORM;
  case VK_FORMAT_R16G16_UINT:
    return rhi::Format::RG16_UINT;
  case VK_FORMAT_R16G16_SINT:
    return rhi::Format::RG16_SINT;
  case VK_FORMAT_R16G16_SFLOAT:
    return rhi::Format::RG16_FLOAT;
  case VK_FORMAT_R16G16B16A16_UNORM:
    return rhi::Format::RGBA16_UNORM;
  case VK_FORMAT_R16G16B16A16_SNORM:
    return rhi::Format::RGBA16_SNORM;
  case VK_FORMAT_R16G16B16A16_UINT:
    return rhi::Format::RGBA16_UINT;
  case VK_FORMAT_R16G16B16A16_SINT:
    return rhi::Format::RGBA16_SINT;
  case VK_FORMAT_R16G16B16A16_SFLOAT:
    return rhi::Format::RGBA16_FLOAT;
  case VK_FORMAT_R32_UINT:
    return rhi::Format::R32_UINT;
  case VK_FORMAT_R32_SINT:
    return rhi::Format::R32_SINT;
  case VK_FORMAT_R32_SFLOAT:
    return rhi::Format::R32_FLOAT;
  case VK_FORMAT_R32G32_UINT:
    return rhi::Format::RG32_UINT;
  case VK_FORMAT_R32G32_SINT:
    return rhi::Format::RG32_SINT;
  case VK_FORMAT_R32G32_SFLOAT:
    return rhi::Format::RG32_FLOAT;
  case VK_FORMAT_R32G32B32_UINT:
    return rhi::Format::RGB32_UINT;
  case VK_FORMAT_R32G32B32_SINT:
    return rhi::Format::RGB32_SINT;
  case VK_FORMAT_R32G32B32_SFLOAT:
    return rhi::Format::RGB32_FLOAT;
  case VK_FORMAT_R32G32B32A32_UINT:
    return rhi::Format::RGBA32_UINT;
  case VK_FORMAT_R32G32B32A32_SINT:
    return rhi::Format::RGBA32_SINT;
  case VK_FORMAT_R32G32B32A32_SFLOAT:
    return rhi::Format::RGBA32_FLOAT;
  case VK_FORMAT_D16_UNORM:
    return rhi::Format::D16_UNORM;
  case VK_FORMAT_D32_SFLOAT:
    return rhi::Format::D32_FLOAT;
  case VK_FORMAT_D24_UNORM_S8_UINT:
    return rhi::Format::D24_UNORM_S8_UINT;
  case VK_FORMAT_D32_SFLOAT_S8_UINT:
    return rhi::Format::D32_FLOAT_S8_UINT;
  default:
    return rhi::Format::Undefined;
  }
}

VkImageLayout toVkImageLayout(rhi::ImageLayout layout) noexcept {
  switch (layout) {
  case rhi::ImageLayout::Undefined:
    return VK_IMAGE_LAYOUT_UNDEFINED;
  case rhi::ImageLayout::General:
    return VK_IMAGE_LAYOUT_GENERAL;
  case rhi::ImageLayout::ColorAttachmentOptimal:
    return VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
  case rhi::ImageLayout::DepthStencilAttachmentOptimal:
    return VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
  case rhi::ImageLayout::DepthStencilReadOnlyOptimal:
    return VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
  case rhi::ImageLayout::ShaderReadOnlyOptimal:
    return VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  case rhi::ImageLayout::TransferSrcOptimal:
    return VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
  case rhi::ImageLayout::TransferDstOptimal:
    return VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
  case rhi::ImageLayout::Preinitialized:
    return VK_IMAGE_LAYOUT_PREINITIALIZED;
  case rhi::ImageLayout::PresentSrc:
    return VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
  default:
    return VK_IMAGE_LAYOUT_UNDEFINED;
  }
}

VkPipelineStageFlags2
toVkPipelineStageFlags2(rhi::PipelineStageFlags stage) noexcept {
  VkPipelineStageFlags2 flags = 0;
  const auto u = static_cast<core::u32>(stage);
  if ((u & static_cast<core::u32>(rhi::PipelineStageFlags::TopOfPipe)) != 0) {
    flags |= VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
  }
  if ((u & static_cast<core::u32>(rhi::PipelineStageFlags::DrawIndirect)) !=
      0) {
    flags |= VK_PIPELINE_STAGE_2_DRAW_INDIRECT_BIT;
  }
  if ((u & static_cast<core::u32>(rhi::PipelineStageFlags::VertexShader)) !=
      0) {
    flags |= VK_PIPELINE_STAGE_2_VERTEX_SHADER_BIT;
  }
  if ((u & static_cast<core::u32>(rhi::PipelineStageFlags::MeshShader)) != 0) {
    flags |= VK_PIPELINE_STAGE_2_MESH_SHADER_BIT_EXT;
  }
  if ((u & static_cast<core::u32>(rhi::PipelineStageFlags::TaskShader)) != 0) {
    flags |= VK_PIPELINE_STAGE_2_TASK_SHADER_BIT_EXT;
  }
  if ((u & static_cast<core::u32>(rhi::PipelineStageFlags::FragmentShader)) !=
      0) {
    flags |= VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT;
  }
  if ((u & static_cast<core::u32>(
               rhi::PipelineStageFlags::EarlyFragmentTests)) != 0) {
    flags |= VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT;
  }
  if ((u & static_cast<core::u32>(
               rhi::PipelineStageFlags::LateFragmentTests)) != 0) {
    flags |= VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
  }
  if ((u & static_cast<core::u32>(
               rhi::PipelineStageFlags::ColorAttachmentOutput)) != 0) {
    flags |= VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
  }
  if ((u & static_cast<core::u32>(rhi::PipelineStageFlags::ComputeShader)) !=
      0) {
    flags |= VK_PIPELINE_STAGE_2_COMPUTE_SHADER_BIT;
  }
  if ((u & static_cast<core::u32>(rhi::PipelineStageFlags::Transfer)) != 0) {
    flags |= VK_PIPELINE_STAGE_2_TRANSFER_BIT;
  }
  if ((u & static_cast<core::u32>(rhi::PipelineStageFlags::BottomOfPipe)) !=
      0) {
    flags |= VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
  }
  if ((u & static_cast<core::u32>(rhi::PipelineStageFlags::AllGraphics)) != 0) {
    flags |= VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT;
  }
  if ((u & static_cast<core::u32>(rhi::PipelineStageFlags::AllCommands)) != 0) {
    flags |= VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
  }
  return flags;
}

VkAccessFlags2 toVkAccessFlags2(rhi::AccessFlags access) noexcept {
  VkAccessFlags2 flags = 0;
  const auto u = static_cast<core::u32>(access);
  if ((u & static_cast<core::u32>(rhi::AccessFlags::IndirectCommandRead)) !=
      0) {
    flags |= VK_ACCESS_2_INDIRECT_COMMAND_READ_BIT;
  }
  if ((u & static_cast<core::u32>(rhi::AccessFlags::IndexRead)) != 0) {
    flags |= VK_ACCESS_2_INDEX_READ_BIT;
  }
  if ((u & static_cast<core::u32>(rhi::AccessFlags::VertexAttributeRead)) !=
      0) {
    flags |= VK_ACCESS_2_VERTEX_ATTRIBUTE_READ_BIT;
  }
  if ((u & static_cast<core::u32>(rhi::AccessFlags::UniformRead)) != 0) {
    flags |= VK_ACCESS_2_UNIFORM_READ_BIT;
  }
  if ((u & static_cast<core::u32>(rhi::AccessFlags::InputAttachmentRead)) !=
      0) {
    flags |= VK_ACCESS_2_INPUT_ATTACHMENT_READ_BIT;
  }
  if ((u & static_cast<core::u32>(rhi::AccessFlags::ShaderRead)) != 0) {
    flags |= VK_ACCESS_2_SHADER_READ_BIT;
  }
  if ((u & static_cast<core::u32>(rhi::AccessFlags::ShaderWrite)) != 0) {
    flags |= VK_ACCESS_2_SHADER_WRITE_BIT;
  }
  if ((u & static_cast<core::u32>(rhi::AccessFlags::ColorAttachmentRead)) !=
      0) {
    flags |= VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT;
  }
  if ((u & static_cast<core::u32>(rhi::AccessFlags::ColorAttachmentWrite)) !=
      0) {
    flags |= VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
  }
  if ((u & static_cast<core::u32>(
               rhi::AccessFlags::DepthStencilAttachmentRead)) != 0) {
    flags |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT;
  }
  if ((u & static_cast<core::u32>(
               rhi::AccessFlags::DepthStencilAttachmentWrite)) != 0) {
    flags |= VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  }
  if ((u & static_cast<core::u32>(rhi::AccessFlags::TransferRead)) != 0) {
    flags |= VK_ACCESS_2_TRANSFER_READ_BIT;
  }
  if ((u & static_cast<core::u32>(rhi::AccessFlags::TransferWrite)) != 0) {
    flags |= VK_ACCESS_2_TRANSFER_WRITE_BIT;
  }
  if ((u & static_cast<core::u32>(rhi::AccessFlags::HostRead)) != 0) {
    flags |= VK_ACCESS_2_HOST_READ_BIT;
  }
  if ((u & static_cast<core::u32>(rhi::AccessFlags::HostWrite)) != 0) {
    flags |= VK_ACCESS_2_HOST_WRITE_BIT;
  }
  if ((u & static_cast<core::u32>(rhi::AccessFlags::MemoryRead)) != 0) {
    flags |= VK_ACCESS_2_MEMORY_READ_BIT;
  }
  if ((u & static_cast<core::u32>(rhi::AccessFlags::MemoryWrite)) != 0) {
    flags |= VK_ACCESS_2_MEMORY_WRITE_BIT;
  }
  return flags;
}

VkPresentModeKHR toVkPresentMode(rhi::PresentMode mode) noexcept {
  switch (mode) {
  case rhi::PresentMode::Immediate:
    return VK_PRESENT_MODE_IMMEDIATE_KHR;
  case rhi::PresentMode::Mailbox:
    return VK_PRESENT_MODE_MAILBOX_KHR;
  case rhi::PresentMode::Fifo:
    return VK_PRESENT_MODE_FIFO_KHR;
  case rhi::PresentMode::FifoRelaxed:
    return VK_PRESENT_MODE_FIFO_RELAXED_KHR;
  default:
    return VK_PRESENT_MODE_FIFO_KHR;
  }
}

rhi::PresentMode fromVkPresentMode(VkPresentModeKHR mode) noexcept {
  switch (mode) {
  case VK_PRESENT_MODE_IMMEDIATE_KHR:
    return rhi::PresentMode::Immediate;
  case VK_PRESENT_MODE_MAILBOX_KHR:
    return rhi::PresentMode::Mailbox;
  case VK_PRESENT_MODE_FIFO_KHR:
    return rhi::PresentMode::Fifo;
  case VK_PRESENT_MODE_FIFO_RELAXED_KHR:
    return rhi::PresentMode::FifoRelaxed;
  default:
    return rhi::PresentMode::Fifo;
  }
}

} // namespace engine::rhi_vulkan
