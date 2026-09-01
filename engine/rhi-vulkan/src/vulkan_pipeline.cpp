#include "vulkan_pipeline.hpp"

#include <utility>
#include <vector>

#include "vulkan_descriptor_set.hpp"

namespace engine::rhi_vulkan {

namespace {

VkPolygonMode toVkPolygonMode(rhi::PolygonMode mode) {
  switch (mode) {
  case rhi::PolygonMode::Fill:
    return VK_POLYGON_MODE_FILL;
  case rhi::PolygonMode::Line:
    return VK_POLYGON_MODE_LINE;
  case rhi::PolygonMode::Point:
    return VK_POLYGON_MODE_POINT;
  default:
    return VK_POLYGON_MODE_FILL;
  }
}

VkCullModeFlags toVkCullMode(rhi::CullMode mode) {
  switch (mode) {
  case rhi::CullMode::None:
    return VK_CULL_MODE_NONE;
  case rhi::CullMode::Front:
    return VK_CULL_MODE_FRONT_BIT;
  case rhi::CullMode::Back:
    return VK_CULL_MODE_BACK_BIT;
  case rhi::CullMode::FrontAndBack:
    return VK_CULL_MODE_FRONT_AND_BACK;
  default:
    return VK_CULL_MODE_BACK_BIT;
  }
}

VkFrontFace toVkFrontFace(rhi::FrontFace face) {
  switch (face) {
  case rhi::FrontFace::CounterClockwise:
    return VK_FRONT_FACE_COUNTER_CLOCKWISE;
  case rhi::FrontFace::Clockwise:
    return VK_FRONT_FACE_CLOCKWISE;
  default:
    return VK_FRONT_FACE_COUNTER_CLOCKWISE;
  }
}

VkCompareOp toVkCompareOp(rhi::CompareOp op) {
  switch (op) {
  case rhi::CompareOp::Never:
    return VK_COMPARE_OP_NEVER;
  case rhi::CompareOp::Less:
    return VK_COMPARE_OP_LESS;
  case rhi::CompareOp::Equal:
    return VK_COMPARE_OP_EQUAL;
  case rhi::CompareOp::LessOrEqual:
    return VK_COMPARE_OP_LESS_OR_EQUAL;
  case rhi::CompareOp::Greater:
    return VK_COMPARE_OP_GREATER;
  case rhi::CompareOp::NotEqual:
    return VK_COMPARE_OP_NOT_EQUAL;
  case rhi::CompareOp::GreaterOrEqual:
    return VK_COMPARE_OP_GREATER_OR_EQUAL;
  case rhi::CompareOp::Always:
    return VK_COMPARE_OP_ALWAYS;
  default:
    return VK_COMPARE_OP_LESS_OR_EQUAL;
  }
}

VkShaderModule createShaderModule(VkDevice device,
                                  std::span<const core::u8> code) {
  if (code.empty()) {
    return VK_NULL_HANDLE;
  }
  VkShaderModuleCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  createInfo.codeSize = code.size();
  createInfo.pCode = reinterpret_cast<const core::u32 *>(code.data());

  VkShaderModule module = VK_NULL_HANDLE;
  if (vkCreateShaderModule(device, &createInfo, nullptr, &module) !=
      VK_SUCCESS) {
    return VK_NULL_HANDLE;
  }
  return module;
}

} // namespace

std::expected<std::unique_ptr<VulkanPipeline>, std::string>
VulkanPipeline::create(VkDevice device, const rhi::PipelineDesc &desc) {
  std::vector<VkDescriptorSetLayout> setLayouts;
  setLayouts.reserve(desc.descriptorSets.size());
  for (const auto *ds : desc.descriptorSets) {
    if (ds != nullptr) {
      const auto &vkDs = static_cast<const VulkanDescriptorSet &>(*ds);
      setLayouts.push_back(vkDs.getVkDescriptorSetLayout());
    }
  }

  VkPipelineLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  layoutInfo.setLayoutCount = static_cast<core::u32>(setLayouts.size());
  layoutInfo.pSetLayouts = setLayouts.empty() ? nullptr : setLayouts.data();

  VkPushConstantRange pushConstantRange{};
  pushConstantRange.stageFlags = VK_SHADER_STAGE_ALL;
  pushConstantRange.offset = 0;
  pushConstantRange.size = 256; // 256 bytes push constants
  layoutInfo.pushConstantRangeCount = 1;
  layoutInfo.pPushConstantRanges = &pushConstantRange;

  VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
  VkResult res =
      vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout);
  if (res != VK_SUCCESS) {
    return std::unexpected(std::string("Failed to create pipeline layout: ") +
                           std::string(vkResultToString(res)));
  }

  if (desc.bindPoint == rhi::PipelineBindPoint::Compute) {
    VkShaderModule compModule =
        createShaderModule(device, desc.computeShaderCode);
    if (compModule == VK_NULL_HANDLE) {
      vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
      return std::unexpected("Failed to create compute shader module");
    }

    VkComputePipelineCreateInfo compInfo{};
    compInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    compInfo.layout = pipelineLayout;
    compInfo.stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    compInfo.stage.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    compInfo.stage.module = compModule;
    compInfo.stage.pName = "main";

    VkPipeline pipeline = VK_NULL_HANDLE;
    res = vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &compInfo,
                                   nullptr, &pipeline);
    vkDestroyShaderModule(device, compModule, nullptr);

    if (res != VK_SUCCESS) {
      vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
      return std::unexpected(
          std::string("Failed to create compute pipeline: ") +
          std::string(vkResultToString(res)));
    }

    return std::unique_ptr<VulkanPipeline>(
        new VulkanPipeline(device, pipeline, pipelineLayout, desc.bindPoint));
  }

  // Graphics Pipeline with Mesh Shader & Dynamic Rendering
  std::vector<VkPipelineShaderStageCreateInfo> stages;
  VkShaderModule taskModule = createShaderModule(device, desc.taskShaderCode);
  if (taskModule != VK_NULL_HANDLE) {
    VkPipelineShaderStageCreateInfo stage{};
    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage = VK_SHADER_STAGE_TASK_BIT_EXT;
    stage.module = taskModule;
    stage.pName = "main";
    stages.push_back(stage);
  }

  VkShaderModule meshModule = createShaderModule(device, desc.meshShaderCode);
  if (meshModule != VK_NULL_HANDLE) {
    VkPipelineShaderStageCreateInfo stage{};
    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage = VK_SHADER_STAGE_MESH_BIT_EXT;
    stage.module = meshModule;
    stage.pName = "main";
    stages.push_back(stage);
  }

  VkShaderModule fragModule =
      createShaderModule(device, desc.fragmentShaderCode);
  if (fragModule != VK_NULL_HANDLE) {
    VkPipelineShaderStageCreateInfo stage{};
    stage.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stage.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stage.module = fragModule;
    stage.pName = "main";
    stages.push_back(stage);
  }

  VkPipelineViewportStateCreateInfo viewportState{};
  viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewportState.viewportCount = 1;
  viewportState.scissorCount = 1;

  VkPipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable =
      desc.rasterizer.depthClampEnable ? VK_TRUE : VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode = toVkPolygonMode(desc.rasterizer.polygonMode);
  rasterizer.lineWidth = desc.rasterizer.lineWidth;
  rasterizer.cullMode = toVkCullMode(desc.rasterizer.cullMode);
  rasterizer.frontFace = toVkFrontFace(desc.rasterizer.frontFace);

  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType =
      VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable = VK_FALSE;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

  const bool hasDepthAttachment =
      (desc.depthAttachmentFormat != rhi::Format::Undefined);
  VkPipelineDepthStencilStateCreateInfo depthStencil{};
  depthStencil.sType =
      VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
  depthStencil.depthTestEnable =
      (hasDepthAttachment && desc.depthStencil.depthTestEnable) ? VK_TRUE
                                                                : VK_FALSE;
  depthStencil.depthWriteEnable =
      (hasDepthAttachment && desc.depthStencil.depthWriteEnable) ? VK_TRUE
                                                                 : VK_FALSE;
  depthStencil.depthCompareOp = toVkCompareOp(desc.depthStencil.depthCompareOp);

  std::vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments(
      desc.colorAttachmentFormats.size());
  for (auto &att : colorBlendAttachments) {
    att.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                         VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    att.blendEnable = VK_FALSE;
  }

  VkPipelineColorBlendStateCreateInfo colorBlending{};
  colorBlending.sType =
      VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.attachmentCount =
      static_cast<core::u32>(colorBlendAttachments.size());
  colorBlending.pAttachments = colorBlendAttachments.data();

  const std::vector<VkDynamicState> dynamicStates = {VK_DYNAMIC_STATE_VIEWPORT,
                                                     VK_DYNAMIC_STATE_SCISSOR};
  VkPipelineDynamicStateCreateInfo dynamicState{};
  dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamicState.dynamicStateCount = static_cast<core::u32>(dynamicStates.size());
  dynamicState.pDynamicStates = dynamicStates.data();

  // Vulkan 1.3 Dynamic Rendering Attachment Formats
  std::vector<VkFormat> vkColorFormats;
  vkColorFormats.reserve(desc.colorAttachmentFormats.size());
  for (auto fmt : desc.colorAttachmentFormats) {
    vkColorFormats.push_back(toVkFormat(fmt));
  }

  VkPipelineRenderingCreateInfo renderingInfo{};
  renderingInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
  renderingInfo.colorAttachmentCount =
      static_cast<core::u32>(vkColorFormats.size());
  renderingInfo.pColorAttachmentFormats = vkColorFormats.data();
  renderingInfo.depthAttachmentFormat = toVkFormat(desc.depthAttachmentFormat);

  VkGraphicsPipelineCreateInfo pipelineInfo{};
  pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipelineInfo.pNext = &renderingInfo;
  pipelineInfo.stageCount = static_cast<core::u32>(stages.size());
  pipelineInfo.pStages = stages.data();
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pDepthStencilState = &depthStencil;
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.pDynamicState = &dynamicState;
  pipelineInfo.layout = pipelineLayout;

  VkPipeline pipeline = VK_NULL_HANDLE;
  res = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo,
                                  nullptr, &pipeline);

  if (taskModule != VK_NULL_HANDLE)
    vkDestroyShaderModule(device, taskModule, nullptr);
  if (meshModule != VK_NULL_HANDLE)
    vkDestroyShaderModule(device, meshModule, nullptr);
  if (fragModule != VK_NULL_HANDLE)
    vkDestroyShaderModule(device, fragModule, nullptr);

  if (res != VK_SUCCESS) {
    vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
    return std::unexpected(std::string("Failed to create graphics pipeline: ") +
                           std::string(vkResultToString(res)));
  }

  return std::unique_ptr<VulkanPipeline>(
      new VulkanPipeline(device, pipeline, pipelineLayout, desc.bindPoint));
}

VulkanPipeline::VulkanPipeline(VkDevice device, VkPipeline pipeline,
                               VkPipelineLayout layout,
                               rhi::PipelineBindPoint bindPoint)
    : m_device(device), m_pipeline(pipeline), m_pipelineLayout(layout),
      m_bindPoint(bindPoint) {}

VulkanPipeline::~VulkanPipeline() {
  if (m_pipeline != VK_NULL_HANDLE && m_device != VK_NULL_HANDLE) {
    vkDestroyPipeline(m_device, m_pipeline, nullptr);
    m_pipeline = VK_NULL_HANDLE;
  }
  if (m_pipelineLayout != VK_NULL_HANDLE && m_device != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
    m_pipelineLayout = VK_NULL_HANDLE;
  }
}

VulkanPipeline::VulkanPipeline(VulkanPipeline &&other) noexcept
    : m_device(std::exchange(other.m_device, VK_NULL_HANDLE)),
      m_pipeline(std::exchange(other.m_pipeline, VK_NULL_HANDLE)),
      m_pipelineLayout(std::exchange(other.m_pipelineLayout, VK_NULL_HANDLE)),
      m_bindPoint(other.m_bindPoint) {}

VulkanPipeline &VulkanPipeline::operator=(VulkanPipeline &&other) noexcept {
  if (this != &other) {
    if (m_pipeline != VK_NULL_HANDLE && m_device != VK_NULL_HANDLE) {
      vkDestroyPipeline(m_device, m_pipeline, nullptr);
    }
    if (m_pipelineLayout != VK_NULL_HANDLE && m_device != VK_NULL_HANDLE) {
      vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
    }
    m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
    m_pipeline = std::exchange(other.m_pipeline, VK_NULL_HANDLE);
    m_pipelineLayout = std::exchange(other.m_pipelineLayout, VK_NULL_HANDLE);
    m_bindPoint = other.m_bindPoint;
  }
  return *this;
}

} // namespace engine::rhi_vulkan
