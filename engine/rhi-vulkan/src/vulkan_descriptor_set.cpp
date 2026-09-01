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

VkDescriptorBindingFlags
toVkDescriptorBindingFlags(rhi::DescriptorBindingFlags flags) {
  VkDescriptorBindingFlags vkFlags = 0;
  const auto u = static_cast<core::u32>(flags);
  if ((u & static_cast<core::u32>(
               rhi::DescriptorBindingFlags::UpdateAfterBind)) != 0) {
    vkFlags |= VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
  }
  if ((u & static_cast<core::u32>(
               rhi::DescriptorBindingFlags::PartiallyBound)) != 0) {
    vkFlags |= VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT;
  }
  if ((u & static_cast<core::u32>(
               rhi::DescriptorBindingFlags::VariableDescriptorCount)) != 0) {
    vkFlags |= VK_DESCRIPTOR_BINDING_VARIABLE_DESCRIPTOR_COUNT_BIT;
  }
  if ((u & static_cast<core::u32>(
               rhi::DescriptorBindingFlags::UpdateUnusedWhilePending)) != 0) {
    vkFlags |= VK_DESCRIPTOR_BINDING_UPDATE_UNUSED_WHILE_PENDING_BIT;
  }
  return vkFlags;
}

} // namespace

std::expected<std::unique_ptr<VulkanDescriptorSet>, std::string>
VulkanDescriptorSet::create(VkDevice device,
                            const rhi::DescriptorSetDesc &desc) {
  std::vector<VkDescriptorSetLayoutBinding> bindings;
  std::vector<VkDescriptorBindingFlags> bindingFlags;
  std::vector<VkDescriptorPoolSize> poolSizes;

  bindings.reserve(desc.bindings.size());
  bindingFlags.reserve(desc.bindings.size());
  poolSizes.reserve(desc.bindings.size());

  bool hasUpdateAfterBind = false;

  for (const auto &b : desc.bindings) {
    VkDescriptorSetLayoutBinding binding{};
    binding.binding = b.binding;
    binding.descriptorType = toVkDescriptorType(b.type);
    binding.descriptorCount = b.count;
    binding.stageFlags = toVkShaderStageFlags(b.stageFlags);
    bindings.push_back(binding);

    const VkDescriptorBindingFlags bf = toVkDescriptorBindingFlags(b.flags);
    bindingFlags.push_back(bf);
    if ((bf & VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT) != 0) {
      hasUpdateAfterBind = true;
    }

    VkDescriptorPoolSize poolSize{};
    poolSize.type = binding.descriptorType;
    poolSize.descriptorCount = b.count;
    poolSizes.push_back(poolSize);
  }

  VkDescriptorSetLayoutBindingFlagsCreateInfo flagsInfo{};
  flagsInfo.sType =
      VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
  flagsInfo.bindingCount = static_cast<core::u32>(bindingFlags.size());
  flagsInfo.pBindingFlags = bindingFlags.data();

  VkDescriptorSetLayoutCreateInfo layoutInfo{};
  layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layoutInfo.pNext = bindingFlags.empty() ? nullptr : &flagsInfo;
  if (hasUpdateAfterBind) {
    layoutInfo.flags |=
        VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
  }
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
  if (hasUpdateAfterBind) {
    poolInfo.flags |= VK_DESCRIPTOR_POOL_CREATE_UPDATE_AFTER_BIND_BIT;
  }
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

  VkPushConstantRange pushConstantRange{};
  pushConstantRange.stageFlags = VK_SHADER_STAGE_ALL;
  pushConstantRange.offset = 0;
  pushConstantRange.size = 256;
  pipeLayoutInfo.pushConstantRangeCount = 1;
  pipeLayoutInfo.pPushConstantRanges = &pushConstantRange;

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
  updateBuffers(binding, 0, std::span<const rhi::BufferBindingInfo>(&info, 1));
}

void VulkanDescriptorSet::updateBuffers(
    core::u32 binding, core::u32 firstElement,
    std::span<const rhi::BufferBindingInfo> infos) {
  if (infos.empty()) {
    return;
  }

  std::vector<VkDescriptorBufferInfo> bufInfos;
  bufInfos.reserve(infos.size());

  VkDescriptorType descType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;

  for (const auto &info : infos) {
    if (info.buffer == nullptr) {
      continue;
    }
    auto &vkBuf = static_cast<VulkanBuffer &>(*info.buffer);
    VkDescriptorBufferInfo bufInfo{};
    bufInfo.buffer = vkBuf.getVkBuffer();
    bufInfo.offset = info.offset;
    bufInfo.range = (info.range == 0) ? VK_WHOLE_SIZE : info.range;
    bufInfos.push_back(bufInfo);

    descType = (vkBuf.getUsage() & rhi::BufferUsageFlags::StorageBuffer) !=
                       rhi::BufferUsageFlags::None
                   ? VK_DESCRIPTOR_TYPE_STORAGE_BUFFER
                   : VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  }

  if (bufInfos.empty()) {
    return;
  }

  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstSet = m_descriptorSet;
  write.dstBinding = binding;
  write.dstArrayElement = firstElement;
  write.descriptorType = descType;
  write.descriptorCount = static_cast<core::u32>(bufInfos.size());
  write.pBufferInfo = bufInfos.data();

  vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
}

void VulkanDescriptorSet::updateTexture(core::u32 binding,
                                        const rhi::TextureBindingInfo &info) {
  updateTextures(binding, 0,
                 std::span<const rhi::TextureBindingInfo>(&info, 1));
}

void VulkanDescriptorSet::updateTextures(
    core::u32 binding, core::u32 firstElement,
    std::span<const rhi::TextureBindingInfo> infos) {
  if (infos.empty()) {
    return;
  }

  std::vector<VkDescriptorImageInfo> imgInfos;
  imgInfos.reserve(infos.size());

  for (const auto &info : infos) {
    if (info.texture == nullptr) {
      continue;
    }
    auto &vkTex = static_cast<VulkanTexture &>(*info.texture);
    VkDescriptorImageInfo imgInfo{};
    imgInfo.imageView = vkTex.getVkImageView();
    imgInfo.imageLayout = toVkImageLayout(info.layout);
    imgInfos.push_back(imgInfo);
  }

  if (imgInfos.empty()) {
    return;
  }

  VkWriteDescriptorSet write{};
  write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
  write.dstSet = m_descriptorSet;
  write.dstBinding = binding;
  write.dstArrayElement = firstElement;
  write.descriptorType = VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
  write.descriptorCount = static_cast<core::u32>(imgInfos.size());
  write.pImageInfo = imgInfos.data();

  vkUpdateDescriptorSets(m_device, 1, &write, 0, nullptr);
}

} // namespace engine::rhi_vulkan
