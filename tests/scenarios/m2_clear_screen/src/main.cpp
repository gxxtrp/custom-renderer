#include <cmath>
#include <memory>
#include <span>
#include <vector>

#include <engine/core/log.hpp>
#include <engine/core/profiler.hpp>
#include <engine/core/types.hpp>
#include <engine/platform/key_codes.hpp>
#include <engine/platform/platform.hpp>
#include <engine/platform/window.hpp>
#include <engine/rhi/rhi.hpp>
#include <engine/rhi_vulkan/vulkan_device.hpp>

#include <tests/common/timer.hpp>

namespace {

using namespace engine::core;
using namespace engine::rhi;

[[nodiscard]] ColorClearValue hsvToRgb(f32 h, f32 s, f32 v) noexcept {
  while (h < 0.0f)
    h += 360.0f;
  while (h >= 360.0f)
    h -= 360.0f;

  const f32 c = v * s;
  const f32 x = c * (1.0f - std::fabs(std::fmod(h / 60.0f, 2.0f) - 1.0f));
  const f32 m = v - c;

  f32 r = 0.0f;
  f32 g = 0.0f;
  f32 b = 0.0f;

  if (h < 60.0f) {
    r = c;
    g = x;
    b = 0.0f;
  } else if (h < 120.0f) {
    r = x;
    g = c;
    b = 0.0f;
  } else if (h < 180.0f) {
    r = 0.0f;
    g = c;
    b = x;
  } else if (h < 240.0f) {
    r = 0.0f;
    g = x;
    b = c;
  } else if (h < 300.0f) {
    r = x;
    g = 0.0f;
    b = c;
  } else {
    r = c;
    g = 0.0f;
    b = x;
  }

  return ColorClearValue{.r = r + m, .g = g + m, .b = b + m, .a = 1.0f};
}

struct FrameContext {
  std::unique_ptr<CommandBuffer> commandBuffer;
  std::unique_ptr<Semaphore> imageAvailableSemaphore;
  std::unique_ptr<Semaphore> renderFinishedSemaphore;
  std::unique_ptr<Fence> inFlightFence;
};

} // namespace

int main(int argc, char **argv) {
  using namespace engine::core;
  using namespace engine::platform;
  using namespace engine::rhi;

  u32 maxFrames = 0;
  for (int i = 1; i < argc; ++i) {
    if (std::string_view(argv[i]) == "--frames" && i + 1 < argc) {
      maxFrames = static_cast<u32>(std::atoi(argv[++i]));
    }
  }

  Log::init(LogLevel::Info);
  ENGINE_LOG_INFO("=================================================");
  ENGINE_LOG_INFO(" Starting Scenario: m2_clear_screen");
  ENGINE_LOG_INFO("=================================================");

  auto initResult = Platform::init();
  if (!initResult) {
    ENGINE_LOG_FATAL("Failed to initialize platform: {}", initResult.error());
  }

  {
    const WindowDesc windowDesc{.title =
                                    "Custom Engine - M2 Clear Screen Scenario",
                                .width = 1280,
                                .height = 720,
                                .resizable = true,
                                .highDpi = true};

    auto windowResult = Window::create(windowDesc);
    if (!windowResult) {
      ENGINE_LOG_FATAL("Failed to create window: {}", windowResult.error());
    }
    Window window = std::move(windowResult.value());

    // Create Vulkan RHI device with validation enabled in debug
#if defined(NDEBUG)
    constexpr bool enableValidation = false;
#else
    constexpr bool enableValidation = true;
#endif

    const DeviceDesc deviceDesc{.enableValidation = enableValidation,
                                .preferDiscreteGpu = true};

    auto deviceResult = engine::rhi_vulkan::VulkanDevice::create(deviceDesc);
    if (!deviceResult) {
      ENGINE_LOG_FATAL("Failed to create Vulkan device: {}",
                       deviceResult.error());
    }
    std::unique_ptr<Device> device = std::move(deviceResult.value());

    ENGINE_LOG_INFO("Device: {}", device->getDeviceName());
    ENGINE_LOG_INFO("Discrete GPU: {}", device->isDiscreteGpu());
    ENGINE_LOG_INFO("Mesh Shaders: {}", device->supportsMeshShaders());
    ENGINE_LOG_INFO("Ray Tracing: {}", device->supportsRayTracing());

    constexpr u32 MAX_FRAMES_IN_FLIGHT = 2;

    const SwapchainDesc swapchainDesc{.windowHandle = window.getNativeHandle(),
                                      .width = window.getWidth(),
                                      .height = window.getHeight(),
                                      .imageCount = MAX_FRAMES_IN_FLIGHT,
                                      .preferredFormat = Format::BGRA8_UNORM,
                                      .preferredPresentMode =
                                          PresentMode::Immediate};

    auto swapchainResult = device->createSwapchain(swapchainDesc);
    if (!swapchainResult) {
      ENGINE_LOG_FATAL("Failed to create Swapchain: {}",
                       swapchainResult.error());
    }
    std::unique_ptr<Swapchain> swapchain = std::move(swapchainResult.value());

    std::vector<FrameContext> frames(MAX_FRAMES_IN_FLIGHT);
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
      auto cmdRes = device->createCommandBuffer({});
      if (!cmdRes) {
        ENGINE_LOG_FATAL("Failed to create command buffer {}: {}", i,
                         cmdRes.error());
      }
      frames[i].commandBuffer = std::move(cmdRes.value());

      auto sem1Res = device->createSemaphore({});
      if (!sem1Res) {
        ENGINE_LOG_FATAL("Failed to create image-available semaphore {}: {}", i,
                         sem1Res.error());
      }
      frames[i].imageAvailableSemaphore = std::move(sem1Res.value());

      auto sem2Res = device->createSemaphore({});
      if (!sem2Res) {
        ENGINE_LOG_FATAL("Failed to create render-finished semaphore {}: {}", i,
                         sem2Res.error());
      }
      frames[i].renderFinishedSemaphore = std::move(sem2Res.value());

      auto fenceRes = device->createFence({.signaled = true});
      if (!fenceRes) {
        ENGINE_LOG_FATAL("Failed to create in-flight fence {}: {}", i,
                         fenceRes.error());
      }
      frames[i].inFlightFence = std::move(fenceRes.value());
    }

    ENGINE_LOG_INFO("Initialized {} frames in flight", MAX_FRAMES_IN_FLIGHT);
    ENGINE_LOG_INFO(
        "Running clear screen scenario. Press ESC or close window to exit.");

    tests::common::Timer timer;
    f32 accumulator = 0.0f;
    u32 frameCounter = 0;
    f32 hue = 0.0f;
    u32 currentFrameIndex = 0;
    u32 totalFrames = 0;

    while (!window.shouldClose()) {
      ENGINE_PROFILE_FRAME();

      const f32 deltaTime = timer.tick();
      accumulator += deltaTime;
      ++frameCounter;
      ++totalFrames;

      if (maxFrames > 0 && totalFrames >= maxFrames) {
        ENGINE_LOG_INFO("Target frame count reached ({}). Exiting cleanly...",
                        maxFrames);
        window.setShouldClose(true);
        break;
      }

      Platform::pollEvents();

      const auto &input = Platform::getInput();
      if (input.isKeyPressed(KeyCode::Escape)) {
        ENGINE_LOG_INFO("Escape pressed. Exiting...");
        window.setShouldClose(true);
        break;
      }

      const u32 curWidth = window.getWidth();
      const u32 curHeight = window.getHeight();
      if (curWidth == 0 || curHeight == 0) {
        // Minimized window, pause rendering
        continue;
      }

      if (curWidth != swapchain->getWidth() ||
          curHeight != swapchain->getHeight()) {
        auto resizeRes = swapchain->resize(curWidth, curHeight);
        if (!resizeRes) {
          ENGINE_LOG_ERROR("Failed to resize swapchain: {}", resizeRes.error());
        }
      }

      // Cycle clear color hue (60 degrees per second)
      hue += 60.0f * deltaTime;
      if (hue >= 360.0f) {
        hue -= 360.0f;
      }
      const ColorClearValue clearColor = hsvToRgb(hue, 0.85f, 0.90f);

      // Wait for this frame's fence
      auto &frame = frames[currentFrameIndex];
      frame.inFlightFence->wait();

      // Acquire next swapchain image
      auto acquireRes =
          swapchain->acquireNextImage(*frame.imageAvailableSemaphore);
      if (!acquireRes) {
        ENGINE_LOG_ERROR("Acquire image error: {}", acquireRes.error());
        break;
      }

      if (acquireRes->status == AcquireStatus::OutOfDate) {
        auto resizeRes = swapchain->resize(curWidth, curHeight);
        if (!resizeRes) {
          ENGINE_LOG_ERROR("Resize failed on OutOfDate: {}", resizeRes.error());
        }
        continue;
      }

      // Reset fence only when we are sure to submit work
      frame.inFlightFence->reset();

      const u32 imageIndex = acquireRes->imageIndex;
      Texture &backBuffer = swapchain->getImage(imageIndex);

      // Record command buffer
      auto &cmd = *frame.commandBuffer;
      cmd.begin();

      // Transition backBuffer to TransferDstOptimal for clear
      const ImageBarrier barrierToDst{.texture = &backBuffer,
                                      .srcAccess = AccessFlags::None,
                                      .dstAccess = AccessFlags::TransferWrite,
                                      .srcStage = PipelineStageFlags::TopOfPipe,
                                      .dstStage = PipelineStageFlags::Transfer,
                                      .oldLayout = ImageLayout::Undefined,
                                      .newLayout =
                                          ImageLayout::TransferDstOptimal};
      cmd.pipelineBarrier({.imageBarriers = std::span(&barrierToDst, 1)});

      // Clear color texture
      cmd.clearColorTexture(backBuffer, clearColor);

      // Transition backBuffer to PresentSrc for presentation
      const ImageBarrier barrierToPresent{
          .texture = &backBuffer,
          .srcAccess = AccessFlags::TransferWrite,
          .dstAccess = AccessFlags::None,
          .srcStage = PipelineStageFlags::Transfer,
          .dstStage = PipelineStageFlags::BottomOfPipe,
          .oldLayout = ImageLayout::TransferDstOptimal,
          .newLayout = ImageLayout::PresentSrc};
      cmd.pipelineBarrier({.imageBarriers = std::span(&barrierToPresent, 1)});

      cmd.end();

      // Submit command buffer
      const Semaphore *waitSems[] = {frame.imageAvailableSemaphore.get()};
      const PipelineStageFlags waitStages[] = {PipelineStageFlags::Transfer};
      const Semaphore *signalSems[] = {frame.renderFinishedSemaphore.get()};

      const SubmitInfo submitInfo{.commandBuffer = &cmd,
                                  .signalFence = frame.inFlightFence.get(),
                                  .waitSemaphores = waitSems,
                                  .waitStages = waitStages,
                                  .signalSemaphores = signalSems};
      device->submit(submitInfo);

      // Present swapchain image
      auto presentRes =
          swapchain->present(imageIndex, *frame.renderFinishedSemaphore);
      if (!presentRes) {
        ENGINE_LOG_ERROR("Present error: {}", presentRes.error());
        break;
      }

      if (presentRes.value() == PresentStatus::OutOfDate ||
          presentRes.value() == PresentStatus::Suboptimal) {
        auto resizeRes = swapchain->resize(curWidth, curHeight);
        if (!resizeRes) {
          ENGINE_LOG_ERROR("Resize failed after present: {}",
                           resizeRes.error());
        }
      }

      // Periodic metrics logging
      if (accumulator >= 1.0f) {
        const f32 fps = static_cast<f32>(frameCounter) / accumulator;
        const f32 frameTimeMs =
            (accumulator / static_cast<f32>(frameCounter)) * 1000.0f;
        ENGINE_LOG_INFO("FPS: {:.1f} | Frame Time: {:.3f} ms | Clear Color "
                        "RGB: ({:.2f}, {:.2f}, {:.2f})",
                        fps, frameTimeMs, clearColor.r, clearColor.g,
                        clearColor.b);
        accumulator = 0.0f;
        frameCounter = 0;
      }

      currentFrameIndex = (currentFrameIndex + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    ENGINE_LOG_INFO("Exiting frame loop. Waiting for device idle...");
    device->waitIdle();

    frames.clear();
    swapchain.reset();
    device.reset();
  }

  Platform::shutdown();
  ENGINE_LOG_INFO("Scenario m2_clear_screen completed cleanly.");
  Log::shutdown();

  return 0;
}
