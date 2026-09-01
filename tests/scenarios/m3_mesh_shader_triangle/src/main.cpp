#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <span>
#include <string_view>
#include <vector>

#include <engine/core/log.hpp>
#include <engine/core/profiler.hpp>
#include <engine/core/types.hpp>
#include <engine/platform/key_codes.hpp>
#include <engine/platform/platform.hpp>
#include <engine/platform/window.hpp>
#include <engine/renderer/render_graph.hpp>
#include <engine/rhi/rhi.hpp>
#include <engine/rhi_vulkan/vulkan_device.hpp>

#include <tests/common/timer.hpp>

namespace {

using namespace engine::core;
using namespace engine::platform;
using namespace engine::renderer;
using namespace engine::rhi;

constexpr u32 maxFramesInFlight = 2;

struct FrameContext {
  std::unique_ptr<CommandBuffer> commandBuffer;
  std::unique_ptr<Semaphore> imageAvailableSemaphore;
  std::unique_ptr<Semaphore> renderFinishedSemaphore;
  std::unique_ptr<Fence> inFlightFence;
};

std::vector<FrameContext> createFrameContexts(Device &device) {
  std::vector<FrameContext> frames(maxFramesInFlight);
  for (u32 i = 0; i < maxFramesInFlight; ++i) {
    auto cmdRes = device.createCommandBuffer({});
    if (!cmdRes) {
      ENGINE_LOG_FATAL("Failed to create command buffer {}: {}", i,
                       cmdRes.error());
    }
    frames[i].commandBuffer = std::move(cmdRes.value());

    auto sem1Res = device.createSemaphore({});
    if (!sem1Res) {
      ENGINE_LOG_FATAL("Failed to create image-available semaphore {}: {}", i,
                       sem1Res.error());
    }
    frames[i].imageAvailableSemaphore = std::move(sem1Res.value());

    auto sem2Res = device.createSemaphore({});
    if (!sem2Res) {
      ENGINE_LOG_FATAL("Failed to create render-finished semaphore {}: {}", i,
                       sem2Res.error());
    }
    frames[i].renderFinishedSemaphore = std::move(sem2Res.value());

    auto fenceRes = device.createFence({.signaled = true});
    if (!fenceRes) {
      ENGINE_LOG_FATAL("Failed to create in-flight fence {}: {}", i,
                       fenceRes.error());
    }
    frames[i].inFlightFence = std::move(fenceRes.value());
  }
  return frames;
}

std::filesystem::path resolveShaderPath(std::string_view filename) {
#if defined(ENGINE_SHADER_DIR)
  const std::filesystem::path buildShaderPath =
      std::filesystem::path(ENGINE_SHADER_DIR) / filename;
  if (std::filesystem::exists(buildShaderPath)) {
    return buildShaderPath;
  }
#endif
  const std::filesystem::path localPath =
      std::filesystem::path("shaders") / filename;
  if (std::filesystem::exists(localPath)) {
    return localPath;
  }

  return std::filesystem::path(filename);
}

std::unique_ptr<Pipeline> createMeshTrianglePipeline(Device &device,
                                                     Format colorFormat) {
  const auto taskPath = resolveShaderPath("mesh_triangle.task.spv");
  const auto meshPath = resolveShaderPath("mesh_triangle.mesh.spv");
  const auto fragPath = resolveShaderPath("mesh_triangle.frag.spv");

  ENGINE_LOG_INFO("Loading task shader: {}", taskPath.string());
  auto taskSpv = loadShaderBytecode(taskPath);
  if (!taskSpv) {
    ENGINE_LOG_FATAL("Failed to load task shader '{}': {}", taskPath.string(),
                     taskSpv.error());
  }

  ENGINE_LOG_INFO("Loading mesh shader: {}", meshPath.string());
  auto meshSpv = loadShaderBytecode(meshPath);
  if (!meshSpv) {
    ENGINE_LOG_FATAL("Failed to load mesh shader '{}': {}", meshPath.string(),
                     meshSpv.error());
  }

  ENGINE_LOG_INFO("Loading fragment shader: {}", fragPath.string());
  auto fragSpv = loadShaderBytecode(fragPath);
  if (!fragSpv) {
    ENGINE_LOG_FATAL("Failed to load fragment shader '{}': {}",
                     fragPath.string(), fragSpv.error());
  }

  const Format colorFormats[] = {colorFormat};
  const PipelineDesc pipelineDesc{
      .taskShaderCode = taskSpv.value(),
      .meshShaderCode = meshSpv.value(),
      .fragmentShaderCode = fragSpv.value(),
      .colorAttachmentFormats = colorFormats,
      .rasterizer = {.polygonMode = PolygonMode::Fill,
                     .cullMode = CullMode::None,
                     .frontFace = FrontFace::CounterClockwise},
      .depthStencil =
          {.depthCompareOp =
               CompareOp::GreaterOrEqual, // Reverse-Z: near is 1.0, far is 0.0
           .depthTestEnable = true,
           .depthWriteEnable = true},
      .depthAttachmentFormat = Format::D32_FLOAT,
      .bindPoint = PipelineBindPoint::Graphics};

  auto pipeRes = device.createPipeline(pipelineDesc);
  if (!pipeRes) {
    ENGINE_LOG_FATAL("Failed to create mesh shader graphics pipeline: {}",
                     pipeRes.error());
  }

  ENGINE_LOG_INFO("Mesh shader graphics pipeline created successfully "
                  "(Reverse-Z D32_SFLOAT, GreaterOrEqual)");
  return std::move(pipeRes.value());
}

bool renderFrame(Window &window, Device &device, Swapchain &swapchain,
                 FrameContext &frame, Pipeline &pipeline,
                 RenderGraph &renderGraph) {
  frame.inFlightFence->wait();

  auto acquireRes = swapchain.acquireNextImage(*frame.imageAvailableSemaphore);
  if (!acquireRes) {
    ENGINE_LOG_ERROR("Acquire image error: {}", acquireRes.error());
    return false;
  }

  const u32 curWidth = window.getWidth();
  const u32 curHeight = window.getHeight();

  if (acquireRes->status == AcquireStatus::OutOfDate) {
    auto resizeRes = swapchain.resize(curWidth, curHeight);
    if (!resizeRes) {
      ENGINE_LOG_ERROR("Resize failed on OutOfDate: {}", resizeRes.error());
    }
    return true;
  }

  frame.inFlightFence->reset();

  Texture &backBuffer = swapchain.getImage(acquireRes->imageIndex);
  auto &cmd = *frame.commandBuffer;

  cmd.begin();

  renderGraph.reset();

  // Import swapchain backbuffer (final layout: PresentSrc for presentation)
  auto swapTarget = renderGraph.importTexture(
      "SwapchainBackbuffer", &backBuffer, ImageLayout::Undefined,
      ImageLayout::PresentSrc);

  // Transient depth buffer for Reverse-Z (VK_FORMAT_D32_SFLOAT)
  auto depthTarget = renderGraph.createTexture(
      RGTextureDesc{.name = "SceneDepth",
                    .width = swapchain.getWidth(),
                    .height = swapchain.getHeight(),
                    .depth = 1,
                    .mipLevels = 1,
                    .arrayLayers = 1,
                    .format = Format::D32_FLOAT,
                    .usage = TextureUsageFlags::DepthStencilAttachment |
                             TextureUsageFlags::TransientAttachment,
                    .dimension = TextureDimension::Texture2D});

  // Declare graphics pass
  renderGraph.addPass(
      "TriangleMeshPass", RGPassType::Graphics,
      [&](RenderPassBuilder &builder) {
        // Clear color to a clean dark slate
        builder.addColorAttachment(
            swapTarget,
            ColorClearValue{.r = 0.06f, .g = 0.08f, .b = 0.12f, .a = 1.0f},
            true);
        // Reverse-Z: clear depth to 0.0f
        builder.setDepthAttachment(
            depthTarget, DepthStencilClearValue{.depth = 0.0f, .stencil = 0},
            true);
      },
      [&](CommandBuffer &passCmd) {
        passCmd.bindPipeline(pipeline);

        const Viewport viewport{.x = 0.0f,
                                .y = 0.0f,
                                .width = static_cast<f32>(curWidth),
                                .height = static_cast<f32>(curHeight),
                                .minDepth = 0.0f,
                                .maxDepth = 1.0f};
        passCmd.setViewport(viewport);

        const Rect2D scissor{
            .offset = {.x = 0, .y = 0},
            .extent = {.width = curWidth, .height = curHeight}};
        passCmd.setScissor(scissor);

        // Dispatch task & mesh shaders: 1 meshlet group
        passCmd.drawMeshTasks(1, 1, 1);
      });

  renderGraph.compile(device);
  renderGraph.execute(device, cmd);

  cmd.end();

  const Semaphore *waitSems[] = {frame.imageAvailableSemaphore.get()};
  const PipelineStageFlags waitStages[] = {
      PipelineStageFlags::ColorAttachmentOutput};
  const Semaphore *signalSems[] = {frame.renderFinishedSemaphore.get()};

  const SubmitInfo submitInfo{.commandBuffer = &cmd,
                              .signalFence = frame.inFlightFence.get(),
                              .waitSemaphores = waitSems,
                              .waitStages = waitStages,
                              .signalSemaphores = signalSems};
  device.submit(submitInfo);

  auto presentRes =
      swapchain.present(acquireRes->imageIndex, *frame.renderFinishedSemaphore);
  if (!presentRes) {
    ENGINE_LOG_ERROR("Present error: {}", presentRes.error());
    return false;
  }

  if (presentRes.value() == PresentStatus::OutOfDate ||
      presentRes.value() == PresentStatus::Suboptimal) {
    auto resizeRes = swapchain.resize(curWidth, curHeight);
    if (!resizeRes) {
      ENGINE_LOG_ERROR("Resize failed after present: {}", resizeRes.error());
    }
  }

  return true;
}

void runScenarioLoop(Window &window, Device &device, Swapchain &swapchain,
                     Pipeline &pipeline, std::vector<FrameContext> &frames,
                     u32 maxFrames) {
  tests::common::Timer timer;
  f32 accumulator = 0.0f;
  u32 frameCounter = 0;
  u32 currentFrameIndex = 0;
  u32 totalFrames = 0;

  RenderGraph renderGraph;

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
      continue;
    }

    if (curWidth != swapchain.getWidth() ||
        curHeight != swapchain.getHeight()) {
      auto resizeRes = swapchain.resize(curWidth, curHeight);
      if (!resizeRes) {
        ENGINE_LOG_ERROR("Failed to resize swapchain: {}", resizeRes.error());
      }
    }

    auto &frame = frames[currentFrameIndex];
    if (!renderFrame(window, device, swapchain, frame, pipeline, renderGraph)) {
      break;
    }

    if (accumulator >= 1.0f) {
      const f32 fps = static_cast<f32>(frameCounter) / accumulator;
      const f32 frameTimeMs =
          (accumulator / static_cast<f32>(frameCounter)) * 1000.0f;
      ENGINE_LOG_INFO(
          "FPS: {:.1f} | Frame Time: {:.3f} ms | Frames Rendered: {}", fps,
          frameTimeMs, totalFrames);
      accumulator = 0.0f;
      frameCounter = 0;
    }

    currentFrameIndex = (currentFrameIndex + 1) % maxFramesInFlight;
  }

  device.waitIdle();
}

} // namespace

int main(int argc, char *argv[]) {
  Log::init();
  ENGINE_LOG_INFO("=== Milestone 3 Scenario: Mesh Shader Triangle (Render "
                  "Graph + Reverse-Z) ===");

  u32 maxFrames = 0;
  u32 windowWidth = 1280;
  u32 windowHeight = 720;

  for (int i = 1; i < argc; ++i) {
    const std::string_view arg(argv[i]);
    if (arg == "--frames" && i + 1 < argc) {
      maxFrames = static_cast<u32>(std::atoi(argv[++i]));
    } else if (arg == "--width" && i + 1 < argc) {
      windowWidth = static_cast<u32>(std::atoi(argv[++i]));
    } else if (arg == "--height" && i + 1 < argc) {
      windowHeight = static_cast<u32>(std::atoi(argv[++i]));
    }
  }

  if (!Platform::init()) {
    ENGINE_LOG_FATAL("Failed to initialize engine platform.");
    return EXIT_FAILURE;
  }

  {
    WindowDesc winDesc{
        .title = "M3: Mesh Shader Triangle (Render Graph + Reverse-Z)",
        .width = windowWidth,
        .height = windowHeight,
        .resizable = true};

    auto windowRes = Window::create(winDesc);
    if (!windowRes) {
      ENGINE_LOG_FATAL("Failed to create platform window: {}",
                       windowRes.error());
      Platform::shutdown();
      return EXIT_FAILURE;
    }
    auto window = std::move(windowRes.value());

    const DeviceDesc deviceDesc{.enableValidation = true,
                                .preferDiscreteGpu = true};
    auto deviceRes = engine::rhi_vulkan::VulkanDevice::create(deviceDesc);
    if (!deviceRes) {
      ENGINE_LOG_FATAL("Failed to initialize Vulkan device: {}",
                       deviceRes.error());
      Platform::shutdown();
      return EXIT_FAILURE;
    }
    auto device = std::move(deviceRes.value());

    if (!device->supportsMeshShaders()) {
      ENGINE_LOG_FATAL("Selected device does not support mesh shaders "
                       "(VK_EXT_mesh_shader).");
      Platform::shutdown();
      return EXIT_FAILURE;
    }

    ENGINE_LOG_INFO("GPU: {} (Mesh Shaders supported)",
                    device->getDeviceName());

    const SwapchainDesc swapDesc{.windowHandle = window.getNativeHandle(),
                                 .width = window.getWidth(),
                                 .height = window.getHeight(),
                                 .imageCount = maxFramesInFlight,
                                 .preferredFormat = Format::BGRA8_UNORM,
                                 .preferredPresentMode =
                                     PresentMode::Immediate};

    auto swapchainRes = device->createSwapchain(swapDesc);
    if (!swapchainRes) {
      ENGINE_LOG_FATAL("Failed to create swapchain: {}", swapchainRes.error());
      Platform::shutdown();
      return EXIT_FAILURE;
    }
    auto swapchain = std::move(swapchainRes.value());

    auto pipeline = createMeshTrianglePipeline(*device, swapchain->getFormat());

    auto frames = createFrameContexts(*device);

    ENGINE_LOG_INFO("Entering frame loop across {} frames in flight...",
                    maxFramesInFlight);
    runScenarioLoop(window, *device, *swapchain, *pipeline, frames, maxFrames);
  }

  Platform::shutdown();
  ENGINE_LOG_INFO("=== Milestone 3 Scenario completed successfully ===");
  return EXIT_SUCCESS;
}
