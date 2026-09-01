#include "vulkan_descriptor_set.hpp"

#include <utility>
#include <vector>

#include "vulkan_buffer.hpp"
#include "vulkan_texture.hpp"

namespace engine::rhi_vulkan {

namespace {

VkDescriptorType toVkDescriptorType(rhi::DescriptorType type) {
  switch (type) {
  case rhi::DescriptorType::UniformBuffer:
    return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  case rhi::DescriptorType::StorageBuffer:
    return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
  case rhi::DescriptorType::SampledTexture:
    return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  case rhi::DescriptorType::StorageTexture:
    return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
  case rhi::DescriptorType::Sampler:
    return VK_DESCRIPTOR_TYPE_SAMPLER;
  default:
    return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  }
}

VkShaderStageFlags toVkShaderStageFlags(rhi::PipelineStageFlags stages) {
  VkShaderStageFlags flags = 0;
  const auto u = static_cast<core::u32>(stages);
  if ((u & static_cast<core::u32>(rhi::PipelineStageFlags::VertexShader)) !=
      0) {
    flags |= VK_SHADER_STAGE_VERTEX_BIT;
  }
  if ((u & static_cast<core::u32>(rhi::PipelineStageFlags::MeshShader)) != 0) {
    flags |= VK_SHADER_STAGE_MESH_BIT_EXT;
  }
  if ((u & static_cast<core::u32>(rhi::PipelineStageFlags::TaskShader)) != 0) {
    flags |= VK_SHADER_STAGE_TASK_BIT_EXT;
  }
  if ((u & static_cast<core::u32>(rhi::PipelineStageFlags::FragmentShader)) !=
      0) {
    flags |= VK_SHADER_STAGE_FRAGMENT_BIT;
  }
  if ((u & static_cast<core::u32>(rhi::PipelineStageFlags::ComputeShader)) !=
      0) {
    flags |= VK_SHADER_STAGE_COMPUTE_BIT;
  }
  if ((u & static_cast<core::u32>(rhi::PipelineStageFlags::AllGraphics)) != 0) {
    flags |= VK_SHADER_STAGE_ALL_GRAPHICS;
  }
  if ((u & static_cast<core::u32>(rhi::PipelineStageFlags::AllCommands)) != 0) {
    flags |= VK_SHADER_STAGE_ALL;
  }
  return flags ? flags : VK_SHADER_STAGE_ALL;
}

} // namespace

std::expected<std::unique_ptr<VulkanDescriptorSet>, std::string>
VulkanDescriptorSet::create(VkDevice device,
                            const rhi::DescriptorSetDesc &desc) {
  std::vector<VkDescriptorSetLayoutBinding> bindings;
  std::vector<VkDescriptorPoolSize> poolSizes;
  bindings.reserve(desc.bindings.size());
  poolSizes.reserve(desc.bindings.size());

  for (const auto &b : desc.bindings) {
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = b.binding;
    binding.descriptorType = toVkDescriptorType(b.type);
    binding.descriptorCount = b.count;
    binding.stageFlags = toVkShaderStageFlags(b.stageFlags);
    bindings.push_back(binding);

    VkDescriptorPoolSize poolSize{};
    poolSize.type = binding.descriptorType;
    poolSize.descriptorCount = b.count;
    poolSizes.push_back(poolSize);
  }

  VkDescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.bindingCount = static_cast<core::u32>(bindings.size());
  layoutInfo.pBindings = bindings.data();

  VkDescriptorSetLayout layout = VK_NULL_HANDLE;
  VkResult res =
      vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &layout);
  if (res != VK_SUCCESS) {
    return std::unexpected(
        std::string("Failed to create descriptor set layout: ") +
        std::string(vkResultToString(res)));
  }

  if (poolSizes.empty()) {
    VkDescriptorPoolSize dummy{};
    dummy.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    dummy.descriptorCount = 1;
    poolSizes.push_back(dummy);
  }

  VkDescriptorPoolCreateInfo poolInfo{};
  poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  poolInfo.maxSets = 1;
  poolInfo.poolSizeCount = static_cast<core::u32>(poolSizes.size());
  poolInfo.pPoolSizes = poolSizes.data();

  VkDescriptorPool pool = VK_NULL_HANDLE;
  res = vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool);
  if (res != VK_SUCCESS) {
    vkDestroyDescriptorSetLayout(device, layout, nullptr);
    return std::unexpected(std::string("Failed to create descriptor pool: ") +
                           std::string(vkResultToString(res)));
  }

  VkDescriptorSetAllocateInfo allocInfo{};
  allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  allocInfo.descriptorPool = pool;
  allocInfo.descriptorSetCount = 1;
  allocInfo.pSetLayouts = &layout;

  VkDescriptorSet set = VK_NULL_HANDLE;
  res = vkAllocateDescriptorSets(device, &allocInfo, &set);
  if (res != VK_SUCCESS) {
    vkDestroyDescriptorPool(device, pool, nullptr);
    vkDestroyDescriptorSetLayout(device, layout, nullptr);
    return std::unexpected(std::string("Failed to allocate descriptor set: ") +
                           std::string(vkResultToString(res)));
  }

  VkPipelineLayoutCreateInfo pipeLayoutInfo{};
  pipeLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipeLayoutInfo.setLayoutCount = 1;
  pipeLayoutInfo.pSetLayouts = &layout;

  VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
  res =
      vkCreatePipelineLayout(device, &pipeLayoutInfo, nullptr, &pipelineLayout);
  if (res != VK_SUCCESS) {
    vkDestroyDescriptorPool(device, pool, nullptr);
    vkDestroyDescriptorSetLayout(device, layout, nullptr);
    return std::unexpected(
        std::string("Failed to create pipeline layout for descriptor set: ") +
        std::string(vkResultToString(res)));
  }

  return std::unique_ptr<VulkanDescriptorSet>(
      new VulkanDescriptorSet(device, pool, layout, set, pipelineLayout));
}

VulkanDescriptorSet::VulkanDescriptorSet(VkDevice device, VkDescriptorPool pool,
                                         VkDescriptorSetLayout layout,
                                         VkDescriptorSet set,
                                         VkPipelineLayout pipelineLayout)
    : m_device(device), m_pool(pool), m_layout(layout), m_descriptorSet(set),
      m_pipelineLayout(pipelineLayout) {}

VulkanDescriptorSet::~VulkanDescriptorSet() {
  if (m_pipelineLayout != VK_NULL_HANDLE && m_device != VK_NULL_HANDLE) {
    vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
    m_pipelineLayout = VK_NULL_HANDLE;
  }
  if (m_pool != VK_NULL_HANDLE && m_device != VK_NULL_HANDLE) {
    vkDestroyDescriptorPool(m_device, m_pool, nullptr);
    m_pool = VK_NULL_HANDLE;
  }
  if (m_layout != VK_NULL_HANDLE && m_device != VK_NULL_HANDLE) {
    vkDestroyDescriptorSetLayout(m_device, m_layout, nullptr);
    m_layout = VK_NULL_HANDLE;
  }
}

VulkanDescriptorSet::VulkanDescriptorSet(VulkanDescriptorSet &&other) noexcept
    : m_device(std::exchange(other.m_device, VK_NULL_HANDLE)),
      m_pool(std::exchange(other.m_pool, VK_NULL_HANDLE)),
      m_layout(std::exchange(other.m_layout, VK_NULL_HANDLE)),
      m_descriptorSet(std::exchange(other.m_descriptorSet, VK_NULL_HANDLE)),
      m_pipelineLayout(std::exchange(other.m_pipelineLayout, VK_NULL_HANDLE)) {}

VulkanDescriptorSet &
VulkanDescriptorSet::operator=(VulkanDescriptorSet &&other) noexcept {
  if (this != &other) {
    if (m_pipelineLayout != VK_NULL_HANDLE && m_device != VK_NULL_HANDLE) {
      vkDestroyPipelineLayout(m_device, m_pipelineLayout, nullptr);
    }
    if (m_pool != VK_NULL_HANDLE && m_device != VK_NULL_HANDLE) {
      vkDestroyDescriptorPool(m_device, m_pool, nullptr);
    }
    if (m_layout != VK_NULL_HANDLE && m_device != VK_NULL_HANDLE) {
      vkDestroyDescriptorSetLayout(m_device, m_layout, nullptr);
    }
    m_device = std::exchange(other.m_device, VK_NULL_HANDLE);
    m_pool = std::exchange(other.m_pool, VK_NULL_HANDLE);
    m_layout = std::exchange(other.m_layout, VK_NULL_HANDLE);
    m_descriptorSet = std::exchange(other.m_descriptorSet, VK_NULL_HANDLE);
    m_pipelineLayout = std::exchange(other.m_pipelineLayout, VK_NULL_HANDLE);
  }
  return *this;
}

void VulkanDescriptorSet::updateBuffer(core::u32 binding,
                                       const rhi::BufferBindingInfo &info) {
  if (info.buffer == nullptr)
    return;
  auto &vkBuf = static_cast<VulkanBuffer &>(*info.buffer);

  VkDescriptorBufferInfo bufInfo{};
  bufInfo.buffer = vkBuf.getVkBuffer();
  bufInfo.offset = info.offset;
  bufInfo.range = (info.range == 0) ? VK_WHOLE_SIZE : info.range;

  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstSet = m_descriptorSet;
  write.dstBinding = binding;
  write.dstArrayElement = 0;
  write.descriptorType =
      (vkBuf.getUsage() & rhi::BufferUsageFlags::StorageBuffer) !=
              rhi::BufferUsageFlags::None
          ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
          : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  write.descriptorCount = 1;
  write.pBufferInfo = &bufInfo;

  vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
}

void VulkanDescriptorSet::updateTexture(core::u32 binding,
                                        const rhi::TextureBindingInfo &info) {
  if (info.texture == nullptr)
    return;
  auto &vkTex = static_cast<VulkanTexture &>(*info.texture);

  VkDescriptorImageInfo imgInfo{};
  imgInfo.imageView = vkTex.getVkImageView();
  imgInfo.imageLayout = toVkImageLayout(info.layout);

  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstSet = m_descriptorSet;
  write.dstBinding = binding;
  write.dstArrayElement = 0;
  write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  write.descriptorCount = 1;
  write.pImageInfo = &imgInfo;

  vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
}

} // namespace engine::rhi_vulkan
