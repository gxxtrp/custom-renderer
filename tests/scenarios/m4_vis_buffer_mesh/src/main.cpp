#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <engine/assets/gltf_loader.hpp>
#include <engine/assets/mesh_asset.hpp>
#include <engine/core/log.hpp>
#include <engine/core/profiler.hpp>
#include <engine/core/types.hpp>
#include <engine/platform/key_codes.hpp>
#include <engine/platform/platform.hpp>
#include <engine/platform/window.hpp>
#include <engine/renderer/render_graph.hpp>
#include <engine/rhi/rhi.hpp>
#include <engine/rhi_vulkan/vulkan_device.hpp>

#include <tests/common/camera.hpp>
#include <tests/common/timer.hpp>

namespace {

using namespace engine::core;
using namespace engine::platform;
using namespace engine::renderer;
using namespace engine::rhi;
using namespace engine::assets;

constexpr u32 maxFramesInFlight = 2;

struct FrameContext {
  std::unique_ptr<CommandBuffer> commandBuffer;
  std::unique_ptr<Semaphore> imageAvailableSemaphore;
  std::unique_ptr<Semaphore> renderFinishedSemaphore;
  std::unique_ptr<Fence> inFlightFence;
  std::unique_ptr<DescriptorSet> deferredDescSet;
};

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

struct GpuMeshBuffers {
  std::unique_ptr<Buffer> meshlets;
  std::unique_ptr<Buffer> vertices;
  std::unique_ptr<Buffer> meshletVertices;
  std::unique_ptr<Buffer> meshletTriangles;
  std::unique_ptr<Buffer> globalIndices;
};

std::expected<GpuMeshBuffers, std::string>
uploadMeshAsset(Device &device, const MeshAsset &mesh) {
  auto createAndUpload = [&](const void *data, usize size,
                             BufferUsageFlags extraUsage, std::string_view name)
      -> std::expected<std::unique_ptr<Buffer>, std::string> {
    const BufferDesc desc{.size = size,
                          .usage = BufferUsageFlags::StorageBuffer |
                                   BufferUsageFlags::TransferDst | extraUsage,
                          .memoryUsage = MemoryUsage::CpuToGpu};

    auto bufRes = device.createBuffer(desc);
    if (!bufRes) {
      return std::unexpected(std::string("Failed to create buffer ") +
                             std::string(name) + ": " + bufRes.error());
    }
    auto buf = std::move(bufRes.value());

    auto mapRes = buf->map();
    if (!mapRes) {
      return std::unexpected(std::string("Failed to map buffer ") +
                             std::string(name) + ": " + mapRes.error());
    }

    std::memcpy(mapRes.value(), data, size);
    buf->unmap();
    return buf;
  };

  GpuMeshBuffers buffers{};

  const auto meshletsSpan = mesh.getMeshlets();
  auto mRes = createAndUpload(meshletsSpan.data(),
                              meshletsSpan.size() * sizeof(Meshlet),
                              BufferUsageFlags::None, "Meshlets");
  if (!mRes) {
    return std::unexpected(mRes.error());
  }
  buffers.meshlets = std::move(mRes.value());

  const auto verticesSpan = mesh.getVertices();
  auto vRes =
      createAndUpload(verticesSpan.data(), verticesSpan.size() * sizeof(Vertex),
                      BufferUsageFlags::None, "Vertices");
  if (!vRes) {
    return std::unexpected(vRes.error());
  }
  buffers.vertices = std::move(vRes.value());

  const auto mvSpan = mesh.getMeshletVertices();
  auto mvRes = createAndUpload(mvSpan.data(), mvSpan.size() * sizeof(u32),
                               BufferUsageFlags::None, "MeshletVertices");
  if (!mvRes) {
    return std::unexpected(mvRes.error());
  }
  buffers.meshletVertices = std::move(mvRes.value());

  const auto mtSpan = mesh.getMeshletTriangles();
  auto mtRes = createAndUpload(mtSpan.data(), mtSpan.size() * sizeof(u32),
                               BufferUsageFlags::None, "MeshletTriangles");
  if (!mtRes) {
    return std::unexpected(mtRes.error());
  }
  buffers.meshletTriangles = std::move(mtRes.value());

  const auto giSpan = mesh.getGlobalIndices();
  auto giRes = createAndUpload(giSpan.data(), giSpan.size() * sizeof(u32),
                               BufferUsageFlags::None, "GlobalIndices");
  if (!giRes) {
    return std::unexpected(giRes.error());
  }
  buffers.globalIndices = std::move(giRes.value());

  ENGINE_LOG_INFO("Uploaded mesh buffers to GPU: Meshlets ({} B), Vertices ({} "
                  "B), Triangles ({} B)",
                  meshletsSpan.size() * sizeof(Meshlet),
                  verticesSpan.size() * sizeof(Vertex),
                  mtSpan.size() * sizeof(u32));

  return buffers;
}

std::unique_ptr<Pipeline> createVisibilityPipeline(Device &device,
                                                   DescriptorSet &visSet) {
  const auto taskPath = resolveShaderPath("meshlet_vis.task.spv");
  const auto meshPath = resolveShaderPath("meshlet_vis.mesh.spv");
  const auto fragPath = resolveShaderPath("meshlet_vis.frag.spv");

  ENGINE_LOG_INFO("Loading visibility shaders: {}, {}, {}", taskPath.string(),
                  meshPath.string(), fragPath.string());
  auto taskSpv = loadShaderBytecode(taskPath);
  if (!taskSpv) {
    ENGINE_LOG_FATAL("Failed to load task shader '{}': {}", taskPath.string(),
                     taskSpv.error());
  }
  auto meshSpv = loadShaderBytecode(meshPath);
  if (!meshSpv) {
    ENGINE_LOG_FATAL("Failed to load mesh shader '{}': {}", meshPath.string(),
                     meshSpv.error());
  }
  auto fragSpv = loadShaderBytecode(fragPath);
  if (!fragSpv) {
    ENGINE_LOG_FATAL("Failed to load fragment shader '{}': {}",
                     fragPath.string(), fragSpv.error());
  }

  const Format colorFormats[] = {Format::RG32_UINT};
  const DescriptorSet *descSets[] = {&visSet};

  const PipelineDesc pipelineDesc{
      .taskShaderCode = taskSpv.value(),
      .meshShaderCode = meshSpv.value(),
      .fragmentShaderCode = fragSpv.value(),
      .colorAttachmentFormats = colorFormats,
      .rasterizer = {.polygonMode = PolygonMode::Fill,
                     .cullMode = CullMode::None,
                     .frontFace = FrontFace::CounterClockwise},
      .depthStencil = {.depthCompareOp = CompareOp::GreaterOrEqual, // Reverse-Z
                       .depthTestEnable = true,
                       .depthWriteEnable = true},
      .depthAttachmentFormat = Format::D32_FLOAT,
      .bindPoint = PipelineBindPoint::Graphics,
      .descriptorSets = descSets};

  auto pipeRes = device.createPipeline(pipelineDesc);
  if (!pipeRes) {
    ENGINE_LOG_FATAL("Failed to create visibility pipeline: {}",
                     pipeRes.error());
  }

  ENGINE_LOG_INFO("Visibility buffer pipeline created successfully (RG32_UINT, "
                  "Reverse-Z D32_SFLOAT)");
  return std::move(pipeRes.value());
}

std::unique_ptr<Pipeline> createDeferredPipeline(Device &device,
                                                 Format swapchainFormat,
                                                 DescriptorSet &deferredSet) {
  const auto meshPath = resolveShaderPath("deferred_texturing.mesh.spv");
  const auto fragPath = resolveShaderPath("deferred_texturing.frag.spv");

  ENGINE_LOG_INFO("Loading deferred resolve shaders: {}, {}", meshPath.string(),
                  fragPath.string());
  auto meshSpv = loadShaderBytecode(meshPath);
  if (!meshSpv) {
    ENGINE_LOG_FATAL("Failed to load deferred mesh shader '{}': {}",
                     meshPath.string(), meshSpv.error());
  }
  auto fragSpv = loadShaderBytecode(fragPath);
  if (!fragSpv) {
    ENGINE_LOG_FATAL("Failed to load deferred fragment shader '{}': {}",
                     fragPath.string(), fragSpv.error());
  }

  const Format colorFormats[] = {swapchainFormat};
  const DescriptorSet *descSets[] = {&deferredSet};

  const PipelineDesc pipelineDesc{
      .meshShaderCode = meshSpv.value(),
      .fragmentShaderCode = fragSpv.value(),
      .colorAttachmentFormats = colorFormats,
      .rasterizer = {.polygonMode = PolygonMode::Fill,
                     .cullMode = CullMode::None},
      .depthStencil = {.depthTestEnable = false, .depthWriteEnable = false},
      .depthAttachmentFormat = Format::Undefined,
      .bindPoint = PipelineBindPoint::Graphics,
      .descriptorSets = descSets};

  auto pipeRes = device.createPipeline(pipelineDesc);
  if (!pipeRes) {
    ENGINE_LOG_FATAL("Failed to create deferred resolve pipeline: {}",
                     pipeRes.error());
  }

  ENGINE_LOG_INFO("Deferred texturing pipeline created successfully");
  return std::move(pipeRes.value());
}

std::vector<FrameContext>
createFrameContexts(Device &device, const DescriptorSetDesc &deferredSetDesc,
                    Buffer &vertices, Buffer &globalIndices) {
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

    auto dsRes = device.createDescriptorSet(deferredSetDesc);
    if (!dsRes) {
      ENGINE_LOG_FATAL("Failed to create deferred descriptor set {}: {}", i,
                       dsRes.error());
    }
    frames[i].deferredDescSet = std::move(dsRes.value());
    frames[i].deferredDescSet->updateBuffer(2, {.buffer = &vertices});
    frames[i].deferredDescSet->updateBuffer(3, {.buffer = &globalIndices});
  }
  return frames;
}

bool renderFrame(Window &window, Device &device, Swapchain &swapchain,
                 FrameContext &frame, Pipeline &visPipeline,
                 Pipeline &deferredPipeline, DescriptorSet &visDescSet,
                 const MeshAsset &mesh, const Mat4 &viewProj, const Mat4 &model,
                 const Vec3 &cameraPos, u32 debugMode,
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

  auto swapTarget = renderGraph.importTexture(
      "SwapchainBackbuffer", &backBuffer, ImageLayout::Undefined,
      ImageLayout::PresentSrc);

  auto visTarget = renderGraph.createTexture(RGTextureDesc{
      .name = "VisibilityBuffer",
      .width = swapchain.getWidth(),
      .height = swapchain.getHeight(),
      .depth = 1,
      .mipLevels = 1,
      .arrayLayers = 1,
      .format = Format::RG32_UINT,
      .usage = TextureUsageFlags::ColorAttachment | TextureUsageFlags::Sampled,
      .dimension = TextureDimension::Texture2D});

  auto depthTarget = renderGraph.createTexture(
      RGTextureDesc{.name = "SceneDepth",
                    .width = swapchain.getWidth(),
                    .height = swapchain.getHeight(),
                    .depth = 1,
                    .mipLevels = 1,
                    .arrayLayers = 1,
                    .format = Format::D32_FLOAT,
                    .usage = TextureUsageFlags::DepthStencilAttachment |
                             TextureUsageFlags::Sampled,
                    .dimension = TextureDimension::Texture2D});

  // Pass 1: Visibility Buffer Rasterization Pass
  renderGraph.addPass(
      "VisibilityPass", RGPassType::Graphics,
      [&](RenderPassBuilder &builder) {
        builder.addColorAttachment(
            visTarget,
            ColorClearValue{.r = 0.0f, .g = 0.0f, .b = 0.0f, .a = 0.0f}, true);
        builder.setDepthAttachment(
            depthTarget, DepthStencilClearValue{.depth = 0.0f, .stencil = 0},
            true);
      },
      [&](CommandBuffer &passCmd) {
        passCmd.bindPipeline(visPipeline);
        passCmd.bindDescriptorSet(visDescSet, 0);

        struct VisibilityPushConstants {
          Mat4 viewProj;
          Mat4 model;
          u32 instanceId;
          u32 meshletOffset;
        } pc{.viewProj = viewProj,
             .model = model,
             .instanceId = 0,
             .meshletOffset = 0};

        passCmd.pushConstants(PipelineStageFlags::MeshShader |
                                  PipelineStageFlags::TaskShader,
                              0, sizeof(pc), &pc);

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

        passCmd.drawMeshTasks(static_cast<u32>(mesh.getMeshletCount()), 1, 1);
      });

  // Pass 2: Deferred Texturing Resolve Pass
  renderGraph.addPass(
      "DeferredTexturingPass", RGPassType::Graphics,
      [&](RenderPassBuilder &builder) {
        builder.read(visTarget, PipelineStageFlags::FragmentShader,
                     AccessFlags::ShaderRead,
                     ImageLayout::ShaderReadOnlyOptimal);
        builder.read(depthTarget, PipelineStageFlags::FragmentShader,
                     AccessFlags::ShaderRead,
                     ImageLayout::DepthStencilReadOnlyOptimal);
        builder.addColorAttachment(
            swapTarget,
            ColorClearValue{.r = 0.0f, .g = 0.0f, .b = 0.0f, .a = 1.0f}, false);
      },
      [&](CommandBuffer &passCmd) {
        passCmd.bindPipeline(deferredPipeline);

        auto *physVis = renderGraph.getPhysicalTexture(visTarget);
        auto *physDepth = renderGraph.getPhysicalTexture(depthTarget);
        frame.deferredDescSet->updateTexture(
            0,
            {.texture = physVis, .layout = ImageLayout::ShaderReadOnlyOptimal});
        frame.deferredDescSet->updateTexture(
            1, {.texture = physDepth,
                .layout = ImageLayout::DepthStencilReadOnlyOptimal});

        passCmd.bindDescriptorSet(*frame.deferredDescSet, 0);

        struct DeferredPushConstants {
          Mat4 viewProj;
          Mat4 model;
          Vec4 cameraPos;
          u32 screenWidth;
          u32 screenHeight;
          u32 debugMode;
          u32 padding;
        } pc{.viewProj = viewProj,
             .model = model,
             .cameraPos = Vec4(cameraPos, 1.0f),
             .screenWidth = curWidth,
             .screenHeight = curHeight,
             .debugMode = debugMode,
             .padding = 0};

        passCmd.pushConstants(PipelineStageFlags::FragmentShader, 0, sizeof(pc),
                              &pc);

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
                     Pipeline &visPipeline, Pipeline &deferredPipeline,
                     DescriptorSet &visDescSet, const MeshAsset &mesh,
                     std::vector<FrameContext> &frames, u32 maxFrames) {
  tests::common::Timer timer;
  tests::common::FlyCamController camera;
  f32 accumulator = 0.0f;
  u32 frameCounter = 0;
  u32 currentFrameIndex = 0;
  u32 totalFrames = 0;
  u32 currentDebugMode =
      1; // 1 = meshlet colors, 2 = triangle colors, 3 = normals, 4 = wireframe

  RenderGraph renderGraph;

  ENGINE_LOG_INFO("Camera fly-cam active. Controls: WASD/QE to move, RMB+drag "
                  "to look. Keys 1-4: Toggle debug "
                  "visualizations.");

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

    if (input.isKeyPressed(KeyCode::Num1)) {
      currentDebugMode = 1;
      ENGINE_LOG_INFO("Switched to Debug Mode 1: Per-Meshlet Colors");
    } else if (input.isKeyPressed(KeyCode::Num2)) {
      currentDebugMode = 2;
      ENGINE_LOG_INFO("Switched to Debug Mode 2: Per-Triangle Colors");
    } else if (input.isKeyPressed(KeyCode::Num3)) {
      currentDebugMode = 3;
      ENGINE_LOG_INFO("Switched to Debug Mode 3: Surface Normals");
    } else if (input.isKeyPressed(KeyCode::Num4)) {
      currentDebugMode = 4;
      ENGINE_LOG_INFO("Switched to Debug Mode 4: Meshlet Wireframe");
    }

    camera.update(input, deltaTime);

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

    const f32 aspect = static_cast<f32>(curWidth) / static_cast<f32>(curHeight);
    const Mat4 view = camera.getViewMatrix();
    const Mat4 proj = camera.getProjectionMatrix(aspect);
    const Mat4 viewProj = proj * view;
    const Mat4 model = Mat4::identity();

    auto &frame = frames[currentFrameIndex];
    if (!renderFrame(window, device, swapchain, frame, visPipeline,
                     deferredPipeline, visDescSet, mesh, viewProj, model,
                     camera.position, currentDebugMode, renderGraph)) {
      break;
    }

    if (accumulator >= 1.0f) {
      const f32 fps = static_cast<f32>(frameCounter) / accumulator;
      const f32 frameTimeMs =
          (accumulator / static_cast<f32>(frameCounter)) * 1000.0f;
      ENGINE_LOG_INFO(
          "FPS: {:.1f} | Frame Time: {:.3f} ms | Frames: {} | Mode: {}", fps,
          frameTimeMs, totalFrames, currentDebugMode);
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
  ENGINE_LOG_INFO(
      "=== Milestone 4 Scenario: Visibility Buffer + Deferred Texturing ===");

  u32 maxFrames = 0;
  u32 windowWidth = 1280;
  u32 windowHeight = 720;
  std::string modelPath;

  for (int i = 1; i < argc; ++i) {
    const std::string_view arg(argv[i]);
    if (arg == "--frames" && i + 1 < argc) {
      maxFrames = static_cast<u32>(std::atoi(argv[++i]));
    } else if (arg == "--width" && i + 1 < argc) {
      windowWidth = static_cast<u32>(std::atoi(argv[++i]));
    } else if (arg == "--height" && i + 1 < argc) {
      windowHeight = static_cast<u32>(std::atoi(argv[++i]));
    } else if (arg == "--model" && i + 1 < argc) {
      modelPath = argv[++i];
    }
  }

  if (!Platform::init()) {
    ENGINE_LOG_FATAL("Failed to initialize engine platform.");
    return EXIT_FAILURE;
  }

  {
    WindowDesc winDesc{.title = "M4: Visibility Buffer + Deferred Texturing",
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

    ENGINE_LOG_INFO("GPU: {} (Mesh Shaders & Descriptor Indexing active)",
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

    // Load 3D model or generate procedural torus knot
    MeshAsset meshAsset;
    if (!modelPath.empty()) {
      ENGINE_LOG_INFO("Loading model from: {}", modelPath);
      auto loadRes = loadGltfMesh(modelPath);
      if (loadRes) {
        meshAsset = std::move(loadRes.value());
      } else {
        ENGINE_LOG_ERROR(
            "Failed to load model '{}': {}. Falling back to procedural knot.",
            modelPath, loadRes.error());
        meshAsset = createProceduralKnot();
      }
    } else {
      ENGINE_LOG_INFO("No model specified via --model. Generating procedural "
                      "clustered torus knot...");
      meshAsset = createProceduralKnot();
    }

    ENGINE_LOG_INFO("Mesh statistics: {} vertices, {} meshlets, {} triangles",
                    meshAsset.getVertexCount(), meshAsset.getMeshletCount(),
                    meshAsset.getTriangleCount());

    auto gpuBuffersRes = uploadMeshAsset(*device, meshAsset);
    if (!gpuBuffersRes) {
      ENGINE_LOG_FATAL("Failed to upload mesh buffers to GPU: {}",
                       gpuBuffersRes.error());
      Platform::shutdown();
      return EXIT_FAILURE;
    }
    auto gpuBuffers = std::move(gpuBuffersRes.value());

    // Create Descriptor Set for Visibility Pass (Set 0)
    const DescriptorBindingDesc visBindings[] = {
        {.stageFlags = PipelineStageFlags::MeshShader,
         .binding = 0,
         .count = 1,
         .type = DescriptorType::StorageBuffer},
        {.stageFlags = PipelineStageFlags::MeshShader,
         .binding = 1,
         .count = 1,
         .type = DescriptorType::StorageBuffer},
        {.stageFlags = PipelineStageFlags::MeshShader,
         .binding = 2,
         .count = 1,
         .type = DescriptorType::StorageBuffer},
        {.stageFlags = PipelineStageFlags::MeshShader,
         .binding = 3,
         .count = 1,
         .type = DescriptorType::StorageBuffer},
    };
    auto visSetRes = device->createDescriptorSet({.bindings = visBindings});
    if (!visSetRes) {
      ENGINE_LOG_FATAL("Failed to create visibility descriptor set: {}",
                       visSetRes.error());
      Platform::shutdown();
      return EXIT_FAILURE;
    }
    auto visSet = std::move(visSetRes.value());
    visSet->updateBuffer(0, {.buffer = gpuBuffers.meshlets.get()});
    visSet->updateBuffer(1, {.buffer = gpuBuffers.vertices.get()});
    visSet->updateBuffer(2, {.buffer = gpuBuffers.meshletVertices.get()});
    visSet->updateBuffer(3, {.buffer = gpuBuffers.meshletTriangles.get()});

    // Descriptor Set Layout for Deferred Pass (Set 0)
    const DescriptorBindingDesc deferredBindings[] = {
        {.stageFlags = PipelineStageFlags::FragmentShader,
         .binding = 0,
         .count = 1,
         .type = DescriptorType::SampledTexture},
        {.stageFlags = PipelineStageFlags::FragmentShader,
         .binding = 1,
         .count = 1,
         .type = DescriptorType::SampledTexture},
        {.stageFlags = PipelineStageFlags::FragmentShader,
         .binding = 2,
         .count = 1,
         .type = DescriptorType::StorageBuffer},
        {.stageFlags = PipelineStageFlags::FragmentShader,
         .binding = 3,
         .count = 1,
         .type = DescriptorType::StorageBuffer},
    };
    const DescriptorSetDesc deferredSetDesc{.bindings = deferredBindings};

    auto frames =
        createFrameContexts(*device, deferredSetDesc, *gpuBuffers.vertices,
                            *gpuBuffers.globalIndices);

    auto visPipeline = createVisibilityPipeline(*device, *visSet);
    auto deferredPipeline = createDeferredPipeline(
        *device, swapchain->getFormat(), *frames[0].deferredDescSet);

    ENGINE_LOG_INFO("Entering frame loop across {} frames in flight...",
                    maxFramesInFlight);
    runScenarioLoop(window, *device, *swapchain, *visPipeline,
                    *deferredPipeline, *visSet, meshAsset, frames, maxFrames);
  }

  Platform::shutdown();
  ENGINE_LOG_INFO("=== Milestone 4 Scenario completed successfully ===");
  return EXIT_SUCCESS;
}
