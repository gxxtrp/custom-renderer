#pragma once

#include <expected>
#include <memory>
#include <span>
#include <string>
#include <string_view>

#include <engine/core/types.hpp>
#include <engine/rhi/buffer.hpp>
#include <engine/rhi/command_buffer.hpp>
#include <engine/rhi/descriptor_set.hpp>
#include <engine/rhi/enums.hpp>
#include <engine/rhi/pipeline.hpp>
#include <engine/rhi/swapchain.hpp>
#include <engine/rhi/sync.hpp>
#include <engine/rhi/texture.hpp>

namespace engine::rhi {

struct SubmitInfo {
  CommandBuffer *commandBuffer{nullptr};
  Fence *signalFence{nullptr};
  std::span<const Semaphore *const> waitSemaphores{};
  std::span<const PipelineStageFlags> waitStages{};
  std::span<const Semaphore *const> signalSemaphores{};
};

struct DeviceDesc {
  bool enableValidation{false};
  bool preferDiscreteGpu{true};
};

class Device {
public:
  virtual ~Device() = default;

  Device(const Device &) = delete;
  Device &operator=(const Device &) = delete;

  Device(Device &&) noexcept = default;
  Device &operator=(Device &&) noexcept = default;

  virtual std::expected<std::unique_ptr<Buffer>, std::string>
  createBuffer(const BufferDesc &desc) = 0;
  virtual std::expected<std::unique_ptr<Texture>, std::string>
  createTexture(const TextureDesc &desc) = 0;
  virtual std::expected<std::unique_ptr<Pipeline>, std::string>
  createPipeline(const PipelineDesc &desc) = 0;
  virtual std::expected<std::unique_ptr<DescriptorSet>, std::string>
  createDescriptorSet(const DescriptorSetDesc &desc) = 0;
  virtual std::expected<std::unique_ptr<Fence>, std::string>
  createFence(const FenceDesc &desc) = 0;
  virtual std::expected<std::unique_ptr<Semaphore>, std::string>
  createSemaphore(const SemaphoreDesc &desc) = 0;
  virtual std::expected<std::unique_ptr<Swapchain>, std::string>
  createSwapchain(const SwapchainDesc &desc) = 0;
  virtual std::expected<std::unique_ptr<CommandBuffer>, std::string>
  createCommandBuffer(const CommandBufferDesc &desc = {}) = 0;

  virtual void submit(const SubmitInfo &submitInfo) = 0;
  virtual void waitIdle() = 0;

  [[nodiscard]] virtual std::string_view getDeviceName() const noexcept = 0;
  [[nodiscard]] virtual bool isDiscreteGpu() const noexcept = 0;
  [[nodiscard]] virtual bool supportsRayTracing() const noexcept = 0;
  [[nodiscard]] virtual bool supportsMeshShaders() const noexcept = 0;

protected:
  Device() = default;
};

} // namespace engine::rhi
