#pragma once

#include <expected>
#include <string>

#include <engine/core/types.hpp>
#include <engine/rhi/enums.hpp>

namespace engine::rhi {

class Fence;
class Semaphore;
class Texture;

enum class AcquireStatus : core::u8 {
  Success = 0,
  Suboptimal,
  OutOfDate,
  Timeout,
  Error
};

enum class PresentStatus : core::u8 {
  Success = 0,
  Suboptimal,
  OutOfDate,
  Error
};

struct AcquireResult {
  core::u32 imageIndex{0};
  AcquireStatus status{AcquireStatus::Success};
};

struct SwapchainDesc {
  void *windowHandle{nullptr};
  core::u32 width{1280};
  core::u32 height{720};
  core::u32 imageCount{2};
  Format preferredFormat{Format::BGRA8_UNORM};
  PresentMode preferredPresentMode{PresentMode::Immediate};
};

class Swapchain {
public:
  virtual ~Swapchain() = default;

  Swapchain(const Swapchain &) = delete;
  Swapchain &operator=(const Swapchain &) = delete;

  Swapchain(Swapchain &&) noexcept = default;
  Swapchain &operator=(Swapchain &&) noexcept = default;

  virtual std::expected<AcquireResult, std::string>
  acquireNextImage(Semaphore &signalSemaphore,
                   Fence *signalFence = nullptr) = 0;
  virtual std::expected<PresentStatus, std::string>
  present(core::u32 imageIndex, Semaphore &waitSemaphore) = 0;
  virtual std::expected<void, std::string> resize(core::u32 width,
                                                  core::u32 height) = 0;

  [[nodiscard]] virtual Texture &getImage(core::u32 index) = 0;
  [[nodiscard]] virtual core::u32 getImageCount() const noexcept = 0;
  [[nodiscard]] virtual core::u32 getWidth() const noexcept = 0;
  [[nodiscard]] virtual core::u32 getHeight() const noexcept = 0;
  [[nodiscard]] virtual Format getFormat() const noexcept = 0;
  [[nodiscard]] virtual PresentMode getPresentMode() const noexcept = 0;

protected:
  Swapchain() = default;
};

} // namespace engine::rhi
