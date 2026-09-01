#pragma once

#include <expected>
#include <memory>
#include <string>
#include <string_view>

#include <engine/core/types.hpp>
#include <engine/rhi/device.hpp>

namespace engine::rhi_vulkan {

class VulkanDevice final : public rhi::Device {
public:
  static std::expected<std::unique_ptr<VulkanDevice>, std::string>
  create(const rhi::DeviceDesc &desc = {});

  ~VulkanDevice() override;

  VulkanDevice(const VulkanDevice &) = delete;
  VulkanDevice &operator=(const VulkanDevice &) = delete;

  VulkanDevice(VulkanDevice &&other) noexcept;
  VulkanDevice &operator=(VulkanDevice &&other) noexcept;

  std::expected<std::unique_ptr<rhi::Buffer>, std::string>
  createBuffer(const rhi::BufferDesc &desc) override;
  std::expected<std::unique_ptr<rhi::Texture>, std::string>
  createTexture(const rhi::TextureDesc &desc) override;
  std::expected<std::unique_ptr<rhi::Sampler>, std::string>
  createSampler(const rhi::SamplerDesc &desc) override;
  std::expected<std::unique_ptr<rhi::Pipeline>, std::string>
  createPipeline(const rhi::PipelineDesc &desc) override;
  std::expected<std::unique_ptr<rhi::DescriptorSet>, std::string>
  createDescriptorSet(const rhi::DescriptorSetDesc &desc) override;
  std::expected<std::unique_ptr<rhi::Fence>, std::string>
  createFence(const rhi::FenceDesc &desc) override;
  std::expected<std::unique_ptr<rhi::Semaphore>, std::string>
  createSemaphore(const rhi::SemaphoreDesc &desc) override;
  std::expected<std::unique_ptr<rhi::Swapchain>, std::string>
  createSwapchain(const rhi::SwapchainDesc &desc) override;
  std::expected<std::unique_ptr<rhi::CommandBuffer>, std::string>
  createCommandBuffer(const rhi::CommandBufferDesc &desc = {}) override;

  void submit(const rhi::SubmitInfo &submitInfo) override;
  void waitIdle() override;

  [[nodiscard]] std::string_view getDeviceName() const noexcept override;
  [[nodiscard]] bool isDiscreteGpu() const noexcept override;
  [[nodiscard]] bool supportsRayTracing() const noexcept override;
  [[nodiscard]] bool supportsMeshShaders() const noexcept override;

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;

  explicit VulkanDevice(std::unique_ptr<Impl> impl);
};

} // namespace engine::rhi_vulkan
