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
#include <engine/core/math.hpp>
#include <engine/core/profiler.hpp>
#include <engine/core/types.hpp>
#include <engine/ecs/world.hpp>
#include <engine/platform/key_codes.hpp>
#include <engine/platform/platform.hpp>
#include <engine/platform/window.hpp>
#include <engine/renderer/render_graph.hpp>
#include <engine/rhi/rhi.hpp>
#include <engine/rhi_vulkan/vulkan_device.hpp>
#include <engine/scene/gpu_scene.hpp>
#include <engine/scene/mesh_component.hpp>
#include <engine/scene/scene_instance.hpp>
#include <engine/scene/transform.hpp>

#include <tests/common/camera.hpp>
#include <tests/common/timer.hpp>

namespace {

using namespace engine::core;
using namespace engine::platform;
using namespace engine::renderer;
using namespace engine::rhi;
using namespace engine::assets;
using namespace engine::ecs;
using namespace engine::scene;

constexpr u32 maxFramesInFlight = 2;

struct SpinAnimation {
  Vec3 axis{0.0f, 1.0f, 0.0f};
  f32 speed{1.0f};
};

struct FrustumPlanes {
  Vec4 planes[6];
};

FrustumPlanes extractFrustumPlanes(const Mat4 &m) noexcept {
  FrustumPlanes fp;
  const Vec4 row0(m(0, 0), m(0, 1), m(0, 2), m(0, 3));
  const Vec4 row1(m(1, 0), m(1, 1), m(1, 2), m(1, 3));
  const Vec4 row2(m(2, 0), m(2, 1), m(2, 2), m(2, 3));
  const Vec4 row3(m(3, 0), m(3, 1), m(3, 2), m(3, 3));

  fp.planes[0] = row3 + row0; // Left
  fp.planes[1] = row3 - row0; // Right
  fp.planes[2] = row3 + row1; // Top (in Vulkan with -Y)
  fp.planes[3] = row3 - row1; // Bottom
  fp.planes[4] = row3 - row2; // Near (Reverse-Z)
  fp.planes[5] = row2;        // Far (Reverse-Z)

  for (auto &plane : fp.planes) {
    const f32 len =
        std::sqrt(plane.x * plane.x + plane.y * plane.y + plane.z * plane.z);
    if (len > 1e-6f) {
      plane = plane * (1.0f / len);
    }
  }
  return fp;
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

struct FrameResources {
  std::unique_ptr<CommandBuffer> commandBuffer;
  std::unique_ptr<Semaphore> imageAvailableSemaphore;
  std::unique_ptr<Semaphore> renderFinishedSemaphore;
  std::unique_ptr<Fence> inFlightFence;

  // Culling and drawing buffers
  std::unique_ptr<Buffer> visibleInstanceIndicesBuffer;
  std::unique_ptr<Buffer> visibleInstanceCountBuffer;
  std::unique_ptr<Buffer> indirectDrawCommandBuffer;
  std::unique_ptr<Buffer> culledMeshletListBuffer;
  std::unique_ptr<Buffer> culledMeshletCountBuffer;
  std::unique_ptr<Buffer> readbackCountBuffer;
  u32 lastSurvivingMeshlets{0};

  // Descriptor Sets
  std::unique_ptr<DescriptorSet> cullResetDescSet;
  std::unique_ptr<DescriptorSet> instanceCullDescSet;
  std::unique_ptr<DescriptorSet> meshletCullDescSet;
  std::unique_ptr<DescriptorSet> visDescSet;
  std::unique_ptr<DescriptorSet> deferredDescSet;
};

struct PipelinePack {
  std::unique_ptr<Pipeline> cullResetPipeline;
  std::unique_ptr<Pipeline> instanceCullPipeline;
  std::unique_ptr<Pipeline> meshletCullPipeline;
  std::unique_ptr<Pipeline> visPipeline;
  std::unique_ptr<Pipeline> deferredPipeline;
};

PipelinePack createPipelines(Device &device, Format swapchainFormat,
                             DescriptorSet &cullResetSet,
                             DescriptorSet &instCullSet,
                             DescriptorSet &meshCullSet, DescriptorSet &visSet,
                             DescriptorSet &deferredSet) {
  PipelinePack pack;

  // 0. Cull Reset Pipeline (Compute)
  {
    const auto spvPath = resolveShaderPath("cull_reset.comp.spv");
    auto spvRes = loadShaderBytecode(spvPath);
    if (!spvRes) {
      ENGINE_LOG_FATAL("Failed to load shader '{}': {}", spvPath.string(),
                       spvRes.error());
    }
    const DescriptorSet *sets[] = {&cullResetSet};
    const PipelineDesc desc{.computeShaderCode = spvRes.value(),
                            .bindPoint = PipelineBindPoint::Compute,
                            .descriptorSets = sets};
    auto pipeRes = device.createPipeline(desc);
    if (!pipeRes) {
      ENGINE_LOG_FATAL("Failed to create cull reset pipeline: {}",
                       pipeRes.error());
    }
    pack.cullResetPipeline = std::move(pipeRes.value());
  }

  // 1. Instance Cull Pipeline (Compute)
  {
    const auto spvPath = resolveShaderPath("instance_cull.comp.spv");
    auto spvRes = loadShaderBytecode(spvPath);
    if (!spvRes) {
      ENGINE_LOG_FATAL("Failed to load shader '{}': {}", spvPath.string(),
                       spvRes.error());
    }
    const DescriptorSet *sets[] = {&instCullSet};
    const PipelineDesc desc{.computeShaderCode = spvRes.value(),
                            .bindPoint = PipelineBindPoint::Compute,
                            .descriptorSets = sets};
    auto pipeRes = device.createPipeline(desc);
    if (!pipeRes) {
      ENGINE_LOG_FATAL("Failed to create instance cull pipeline: {}",
                       pipeRes.error());
    }
    pack.instanceCullPipeline = std::move(pipeRes.value());
  }

  // 2. Meshlet Cull Pipeline (Compute)
  {
    const auto spvPath = resolveShaderPath("meshlet_cull.comp.spv");
    auto spvRes = loadShaderBytecode(spvPath);
    if (!spvRes) {
      ENGINE_LOG_FATAL("Failed to load shader '{}': {}", spvPath.string(),
                       spvRes.error());
    }
    const DescriptorSet *sets[] = {&meshCullSet};
    const PipelineDesc desc{.computeShaderCode = spvRes.value(),
                            .bindPoint = PipelineBindPoint::Compute,
                            .descriptorSets = sets};
    auto pipeRes = device.createPipeline(desc);
    if (!pipeRes) {
      ENGINE_LOG_FATAL("Failed to create meshlet cull pipeline: {}",
                       pipeRes.error());
    }
    pack.meshletCullPipeline = std::move(pipeRes.value());
  }

  // 3. Visibility Pipeline (Mesh Shader Graphics)
  {
    const auto taskPath = resolveShaderPath("meshlet_vis_gpu_scene.task.spv");
    const auto meshPath = resolveShaderPath("meshlet_vis_gpu_scene.mesh.spv");
    const auto fragPath = resolveShaderPath("meshlet_vis_gpu_scene.frag.spv");

    auto taskSpv = loadShaderBytecode(taskPath);
    auto meshSpv = loadShaderBytecode(meshPath);
    auto fragSpv = loadShaderBytecode(fragPath);

    if (!taskSpv || !meshSpv || !fragSpv) {
      ENGINE_LOG_FATAL("Failed to load visibility shaders");
    }

    const Format colorFormats[] = {Format::RG32_UINT};
    const DescriptorSet *sets[] = {&visSet};

    const PipelineDesc desc{
        .taskShaderCode = taskSpv.value(),
        .meshShaderCode = meshSpv.value(),
        .fragmentShaderCode = fragSpv.value(),
        .colorAttachmentFormats = colorFormats,
        .rasterizer = {.polygonMode = PolygonMode::Fill,
                       .cullMode = CullMode::None,
                       .frontFace = FrontFace::CounterClockwise},
        .depthStencil = {.depthCompareOp =
                             CompareOp::GreaterOrEqual, // Reverse-Z
                         .depthTestEnable = true,
                         .depthWriteEnable = true},
        .depthAttachmentFormat = Format::D32_FLOAT,
        .bindPoint = PipelineBindPoint::Graphics,
        .descriptorSets = sets};

    auto pipeRes = device.createPipeline(desc);
    if (!pipeRes) {
      ENGINE_LOG_FATAL("Failed to create visibility pipeline: {}",
                       pipeRes.error());
    }
    pack.visPipeline = std::move(pipeRes.value());
  }

  // 4. Deferred Texturing Pipeline (Fullscreen Resolve)
  {
    const auto meshPath =
        resolveShaderPath("deferred_texturing_gpu_scene.mesh.spv");
    const auto fragPath =
        resolveShaderPath("deferred_texturing_gpu_scene.frag.spv");

    auto meshSpv = loadShaderBytecode(meshPath);
    auto fragSpv = loadShaderBytecode(fragPath);

    if (!meshSpv || !fragSpv) {
      ENGINE_LOG_FATAL("Failed to load deferred shaders");
    }

    const Format colorFormats[] = {swapchainFormat};
    const DescriptorSet *sets[] = {&deferredSet};

    const PipelineDesc desc{
        .meshShaderCode = meshSpv.value(),
        .fragmentShaderCode = fragSpv.value(),
        .colorAttachmentFormats = colorFormats,
        .rasterizer = {.polygonMode = PolygonMode::Fill,
                       .cullMode = CullMode::None},
        .depthStencil = {.depthTestEnable = false, .depthWriteEnable = false},
        .depthAttachmentFormat = Format::Undefined,
        .bindPoint = PipelineBindPoint::Graphics,
        .descriptorSets = sets};

    auto pipeRes = device.createPipeline(desc);
    if (!pipeRes) {
      ENGINE_LOG_FATAL("Failed to create deferred resolve pipeline: {}",
                       pipeRes.error());
    }
    pack.deferredPipeline = std::move(pipeRes.value());
  }

  ENGINE_LOG_INFO("All compute culling, mesh shader, and resolve pipelines "
                  "created successfully");
  return pack;
}

std::vector<FrameResources> createFrameResources(Device &device,
                                                 GpuScene &gpuScene,
                                                 u32 maxInstances,
                                                 u32 maxMeshlets) {
  std::vector<FrameResources> frames(maxFramesInFlight);

  for (u32 i = 0; i < maxFramesInFlight; ++i) {
    auto &f = frames[i];

    auto cmdRes = device.createCommandBuffer({});
    if (!cmdRes) {
      ENGINE_LOG_FATAL("Failed to create command buffer");
    }
    f.commandBuffer = std::move(cmdRes.value());

    auto sem1 = device.createSemaphore({});
    auto sem2 = device.createSemaphore({});
    auto fence = device.createFence({.signaled = true});

    if (!sem1 || !sem2 || !fence) {
      ENGINE_LOG_FATAL("Failed to create synchronization primitives");
    }
    f.imageAvailableSemaphore = std::move(sem1.value());
    f.renderFinishedSemaphore = std::move(sem2.value());
    f.inFlightFence = std::move(fence.value());

    // Culling output buffers
    const auto instIndicesDesc =
        BufferDesc{.size = maxInstances * sizeof(u32),
                   .usage = BufferUsageFlags::StorageBuffer,
                   .memoryUsage = MemoryUsage::GpuOnly};
    f.visibleInstanceIndicesBuffer =
        device.createBuffer(instIndicesDesc).value();

    const auto instCountDesc =
        BufferDesc{.size = sizeof(u32),
                   .usage = BufferUsageFlags::StorageBuffer,
                   .memoryUsage = MemoryUsage::GpuOnly};
    f.visibleInstanceCountBuffer = device.createBuffer(instCountDesc).value();

    const auto indirectDrawDesc = BufferDesc{
        .size = sizeof(u32) *
                4, // VkDrawMeshTasksIndirectCommandEXT (12 bytes, 16B padded)
        .usage = BufferUsageFlags::StorageBuffer | BufferUsageFlags::Indirect |
                 BufferUsageFlags::TransferDst | BufferUsageFlags::TransferSrc,
        .memoryUsage = MemoryUsage::GpuOnly};
    f.indirectDrawCommandBuffer = device.createBuffer(indirectDrawDesc).value();

    const auto culledMeshletDesc =
        BufferDesc{.size = std::max(maxMeshlets, 1024u) * sizeof(u32) * 2,
                   .usage = BufferUsageFlags::StorageBuffer,
                   .memoryUsage = MemoryUsage::GpuOnly};
    f.culledMeshletListBuffer = device.createBuffer(culledMeshletDesc).value();

    const auto culledCountDesc =
        BufferDesc{.size = sizeof(u32),
                   .usage = BufferUsageFlags::StorageBuffer,
                   .memoryUsage = MemoryUsage::GpuOnly};
    f.culledMeshletCountBuffer = device.createBuffer(culledCountDesc).value();

    const auto readbackDesc = BufferDesc{.size = sizeof(u32) * 4,
                                         .usage = BufferUsageFlags::TransferDst,
                                         .memoryUsage = MemoryUsage::GpuToCpu};
    f.readbackCountBuffer = device.createBuffer(readbackDesc).value();

    // 0. Cull Reset Descriptor Set (Set 0)
    const DescriptorBindingDesc resetBindings[] = {
        {.stageFlags = PipelineStageFlags::ComputeShader,
         .binding = 0,
         .count = 1,
         .type = DescriptorType::StorageBuffer},
        {.stageFlags = PipelineStageFlags::ComputeShader,
         .binding = 1,
         .count = 1,
         .type = DescriptorType::StorageBuffer},
        {.stageFlags = PipelineStageFlags::ComputeShader,
         .binding = 2,
         .count = 1,
         .type = DescriptorType::StorageBuffer},
    };
    f.cullResetDescSet =
        device.createDescriptorSet({.bindings = resetBindings}).value();
    f.cullResetDescSet->updateBuffer(
        0, {.buffer = f.visibleInstanceCountBuffer.get()});
    f.cullResetDescSet->updateBuffer(
        1, {.buffer = f.indirectDrawCommandBuffer.get()});
    f.cullResetDescSet->updateBuffer(
        2, {.buffer = f.culledMeshletCountBuffer.get()});

    // 1. Instance Cull Descriptor Set (Set 0)
    const DescriptorBindingDesc instCullBindings[] = {
        {.stageFlags = PipelineStageFlags::ComputeShader,
         .binding = 0,
         .count = 1,
         .type = DescriptorType::StorageBuffer},
        {.stageFlags = PipelineStageFlags::ComputeShader,
         .binding = 1,
         .count = 1,
         .type = DescriptorType::StorageBuffer},
        {.stageFlags = PipelineStageFlags::ComputeShader,
         .binding = 2,
         .count = 1,
         .type = DescriptorType::StorageBuffer},
        {.stageFlags = PipelineStageFlags::ComputeShader,
         .binding = 3,
         .count = 1,
         .type = DescriptorType::StorageBuffer},
    };
    f.instanceCullDescSet =
        device.createDescriptorSet({.bindings = instCullBindings}).value();
    f.instanceCullDescSet->updateBuffer(
        0, {.buffer = gpuScene.getInstanceBuffer()});
    f.instanceCullDescSet->updateBuffer(
        1, {.buffer = f.visibleInstanceIndicesBuffer.get()});
    f.instanceCullDescSet->updateBuffer(
        2, {.buffer = f.visibleInstanceCountBuffer.get()});
    f.instanceCullDescSet->updateBuffer(
        3, {.buffer = f.indirectDrawCommandBuffer.get()});

    // 2. Meshlet Cull Descriptor Set (Set 0)
    const DescriptorBindingDesc meshCullBindings[] = {
        {.stageFlags = PipelineStageFlags::ComputeShader,
         .binding = 0,
         .count = 1,
         .type = DescriptorType::StorageBuffer},
        {.stageFlags = PipelineStageFlags::ComputeShader,
         .binding = 1,
         .count = 1,
         .type = DescriptorType::StorageBuffer},
        {.stageFlags = PipelineStageFlags::ComputeShader,
         .binding = 2,
         .count = 1,
         .type = DescriptorType::StorageBuffer},
        {.stageFlags = PipelineStageFlags::ComputeShader,
         .binding = 3,
         .count = 1,
         .type = DescriptorType::StorageBuffer},
        {.stageFlags = PipelineStageFlags::ComputeShader,
         .binding = 4,
         .count = 1,
         .type = DescriptorType::StorageBuffer},
        {.stageFlags = PipelineStageFlags::ComputeShader,
         .binding = 5,
         .count = 1,
         .type = DescriptorType::StorageBuffer},
        {.stageFlags = PipelineStageFlags::ComputeShader,
         .binding = 6,
         .count = 1,
         .type = DescriptorType::StorageBuffer},
    };
    f.meshletCullDescSet =
        device.createDescriptorSet({.bindings = meshCullBindings}).value();
    f.meshletCullDescSet->updateBuffer(
        0, {.buffer = gpuScene.getInstanceBuffer()});
    f.meshletCullDescSet->updateBuffer(1,
                                       {.buffer = gpuScene.getMeshletBuffer()});
    f.meshletCullDescSet->updateBuffer(
        2, {.buffer = f.visibleInstanceIndicesBuffer.get()});
    f.meshletCullDescSet->updateBuffer(
        3, {.buffer = f.visibleInstanceCountBuffer.get()});
    f.meshletCullDescSet->updateBuffer(
        4, {.buffer = f.indirectDrawCommandBuffer.get()});
    f.meshletCullDescSet->updateBuffer(
        5, {.buffer = f.culledMeshletListBuffer.get()});
    f.meshletCullDescSet->updateBuffer(
        6, {.buffer = f.culledMeshletCountBuffer.get()});

    // 3. Visibility Descriptor Set (Set 0)
    const DescriptorBindingDesc visBindings[] = {
        {.stageFlags =
             PipelineStageFlags::MeshShader | PipelineStageFlags::TaskShader,
         .binding = 0,
         .count = 1,
         .type = DescriptorType::StorageBuffer},
        {.stageFlags =
             PipelineStageFlags::MeshShader | PipelineStageFlags::TaskShader,
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
        {.stageFlags = PipelineStageFlags::MeshShader,
         .binding = 4,
         .count = 1,
         .type = DescriptorType::StorageBuffer},
        {.stageFlags = PipelineStageFlags::TaskShader,
         .binding = 5,
         .count = 1,
         .type = DescriptorType::StorageBuffer},
    };
    f.visDescSet =
        device.createDescriptorSet({.bindings = visBindings}).value();
    f.visDescSet->updateBuffer(0, {.buffer = gpuScene.getInstanceBuffer()});
    f.visDescSet->updateBuffer(1, {.buffer = gpuScene.getMeshletBuffer()});
    f.visDescSet->updateBuffer(2, {.buffer = gpuScene.getVertexBuffer()});
    f.visDescSet->updateBuffer(3,
                               {.buffer = gpuScene.getMeshletVertexBuffer()});
    f.visDescSet->updateBuffer(4,
                               {.buffer = gpuScene.getMeshletTriangleBuffer()});
    f.visDescSet->updateBuffer(5, {.buffer = f.culledMeshletListBuffer.get()});

    // 4. Deferred Descriptor Set (Set 0)
    const DescriptorBindingDesc defBindings[] = {
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
        {.stageFlags = PipelineStageFlags::FragmentShader,
         .binding = 4,
         .count = 1,
         .type = DescriptorType::StorageBuffer},
    };
    f.deferredDescSet =
        device.createDescriptorSet({.bindings = defBindings}).value();
    f.deferredDescSet->updateBuffer(2, {.buffer = gpuScene.getVertexBuffer()});
    f.deferredDescSet->updateBuffer(
        3, {.buffer = gpuScene.getGlobalIndexBuffer()});
    f.deferredDescSet->updateBuffer(4,
                                    {.buffer = gpuScene.getInstanceBuffer()});
  }

  return frames;
}

void populateEcsScene(World &world, const MeshAsset &meshAsset,
                      const RegisteredMesh &regMesh, u32 instanceCount) {
  ENGINE_LOG_INFO("Populating ECS world with {} instances...", instanceCount);

  // Compute grid dimensions
  const u32 gridDimX =
      static_cast<u32>(std::ceil(std::cbrt(instanceCount * 1.5f)));
  const u32 gridDimY = std::max(4u, gridDimX / 2);
  const u32 gridDimZ = static_cast<u32>(
      std::ceil(static_cast<f32>(instanceCount) / (gridDimX * gridDimY)));

  constexpr f32 spacing = 3.5f;
  const f32 offsetX = (static_cast<f32>(gridDimX) - 1.0f) * spacing * 0.5f;
  const f32 offsetY = (static_cast<f32>(gridDimY) - 1.0f) * spacing * 0.5f;
  const f32 offsetZ = (static_cast<f32>(gridDimZ) - 1.0f) * spacing * 0.5f;

  u32 created = 0;
  for (u32 z = 0; z < gridDimZ && created < instanceCount; ++z) {
    for (u32 y = 0; y < gridDimY && created < instanceCount; ++y) {
      for (u32 x = 0; x < gridDimX && created < instanceCount; ++x) {
        Entity entity = world.createEntity();

        const Vec3 pos(static_cast<f32>(x) * spacing - offsetX,
                       static_cast<f32>(y) * spacing - offsetY,
                       static_cast<f32>(z) * spacing - offsetZ);

        const f32 rotAngle = static_cast<f32>(created) * 0.17f;
        const Quat rot = Quat::fromAxisAngle(
            Vec3(std::sin(rotAngle), std::cos(rotAngle * 0.7f), 0.5f)
                .normalized(),
            rotAngle);

        const f32 scaleFactor =
            0.8f + 0.4f * std::sin(static_cast<f32>(created) * 0.23f);
        const Vec3 scale(scaleFactor, scaleFactor, scaleFactor);

        world.addComponent(entity, Transform{.position = pos,
                                             .rotation = rot,
                                             .scale = scale,
                                             .dirty = true});

        world.addComponent(entity, MeshComponent(&meshAsset, regMesh.meshId,
                                                 regMesh.meshletOffset));

        // 50% of the entities are dynamically animated
        if (created % 2 == 0) {
          const Vec3 spinAxis(std::sin(static_cast<f32>(created)), 1.0f,
                              std::cos(static_cast<f32>(created) * 0.5f));
          world.addComponent(
              entity,
              SpinAnimation{.axis = spinAxis.normalized(),
                            .speed =
                                0.5f + static_cast<f32>(created % 5) * 0.3f});
        }

        ++created;
      }
    }
  }

  ENGINE_LOG_INFO("Successfully spawned {} ECS entities", created);
}

void updateEcsSystems(World &world, f32 deltaTime) {
  ENGINE_PROFILE_ZONE_NAMED("Ecs::UpdateSystems");

  // SpinAnimation System: rotates animated entities and marks transforms dirty
  auto spinQuery = world.view<Transform, SpinAnimation>();
  spinQuery.each(
      [&](Entity /*entity*/, Transform &transform, const SpinAnimation &spin) {
        const Quat deltaRot =
            Quat::fromAxisAngle(spin.axis, spin.speed * deltaTime);
        transform.rotation = (deltaRot * transform.rotation).normalized();
        transform.dirty = true;
      });
}

bool renderSceneFrame(Window &window, Device &device, Swapchain &swapchain,
                      FrameResources &frame, PipelinePack &pipelines,
                      GpuScene &gpuScene, const Mat4 &viewProj,
                      const Vec3 &cameraPos, u32 debugMode, bool enableConeCull,
                      RenderGraph &renderGraph) {
  frame.inFlightFence->wait();

  if (frame.readbackCountBuffer) {
    auto mapRes = frame.readbackCountBuffer->map();
    if (mapRes) {
      const auto *ptr = static_cast<const u32 *>(mapRes.value());
      frame.lastSurvivingMeshlets = ptr[0];
      frame.readbackCountBuffer->unmap();
    }
  }

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

  // 1. Upload sparse delta packets to persistent GPU scene buffer
  gpuScene.uploadDelta(device, cmd);

  // Update descriptor set instance buffer bindings in case capacity reallocated
  frame.instanceCullDescSet->updateBuffer(
      0, {.buffer = gpuScene.getInstanceBuffer()});
  frame.meshletCullDescSet->updateBuffer(
      0, {.buffer = gpuScene.getInstanceBuffer()});
  frame.visDescSet->updateBuffer(0, {.buffer = gpuScene.getInstanceBuffer()});
  frame.deferredDescSet->updateBuffer(4,
                                      {.buffer = gpuScene.getInstanceBuffer()});

  const FrustumPlanes frustum = extractFrustumPlanes(viewProj);
  const u32 instanceCount = gpuScene.getInstanceCount();
  const u32 meshletsPerInstance =
      gpuScene.getRegisteredMeshes().empty()
          ? 0
          : gpuScene.getRegisteredMeshes()[0].meshletCount;

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

  auto indirectCmdTarget = renderGraph.importBuffer(
      "IndirectDrawCommand", frame.indirectDrawCommandBuffer.get());
  auto culledMeshletTarget = renderGraph.importBuffer(
      "CulledMeshletList", frame.culledMeshletListBuffer.get());

  // Pass 0: Reset Culling Counters and Draw Indirect Arguments (Compute)
  renderGraph.addPass(
      "CullResetPass", RGPassType::Compute,
      [&](RenderPassBuilder &builder) {
        builder.write(indirectCmdTarget, PipelineStageFlags::ComputeShader,
                      AccessFlags::ShaderWrite);
      },
      [&](CommandBuffer &passCmd) {
        passCmd.bindPipeline(*pipelines.cullResetPipeline);
        passCmd.bindDescriptorSet(*frame.cullResetDescSet, 0);
        passCmd.dispatch(1, 1, 1);
      });

  // Pass 1: Instance Frustum Culling (Compute)
  renderGraph.addPass(
      "InstanceCullPass", RGPassType::Compute,
      [&](RenderPassBuilder &builder) {
        builder.write(indirectCmdTarget, PipelineStageFlags::ComputeShader,
                      AccessFlags::ShaderWrite);
      },
      [&](CommandBuffer &passCmd) {
        passCmd.bindPipeline(*pipelines.instanceCullPipeline);
        passCmd.bindDescriptorSet(*frame.instanceCullDescSet, 0);

        struct InstanceCullPushConstants {
          Vec4 frustumPlanes[6];
          Vec4 cameraPos;
          u32 instanceCount;
          u32 padding;
        } pc{.frustumPlanes = {frustum.planes[0], frustum.planes[1],
                               frustum.planes[2], frustum.planes[3],
                               frustum.planes[4], frustum.planes[5]},
             .cameraPos = Vec4(cameraPos, 1.0f),
             .instanceCount = instanceCount,
             .padding = 0};

        passCmd.pushConstants(PipelineStageFlags::ComputeShader, 0, sizeof(pc),
                              &pc);

        const u32 groupCountX = (instanceCount + 63) / 64;
        passCmd.dispatch(groupCountX, 1, 1);
      });

  // Pass 2: Meshlet Frustum + Backface Normal Cone Culling (Compute)
  renderGraph.addPass(
      "MeshletCullPass", RGPassType::Compute,
      [&](RenderPassBuilder &builder) {
        builder.write(indirectCmdTarget, PipelineStageFlags::ComputeShader,
                      AccessFlags::ShaderWrite);
        builder.write(culledMeshletTarget, PipelineStageFlags::ComputeShader,
                      AccessFlags::ShaderWrite);
      },
      [&](CommandBuffer &passCmd) {
        passCmd.bindPipeline(*pipelines.meshletCullPipeline);
        passCmd.bindDescriptorSet(*frame.meshletCullDescSet, 0);

        struct MeshletCullPushConstants {
          Vec4 frustumPlanes[6];
          Vec4 cameraPos;
          u32 instanceCount;
          u32 meshletsPerInstance;
          u32 enableConeCull;
          u32 padding;
        } pc{.frustumPlanes = {frustum.planes[0], frustum.planes[1],
                               frustum.planes[2], frustum.planes[3],
                               frustum.planes[4], frustum.planes[5]},
             .cameraPos = Vec4(cameraPos, 1.0f),
             .instanceCount = instanceCount,
             .meshletsPerInstance = meshletsPerInstance,
             .enableConeCull = enableConeCull ? 1u : 0u,
             .padding = 0};

        passCmd.pushConstants(PipelineStageFlags::ComputeShader, 0, sizeof(pc),
                              &pc);

        const u32 totalThreads = instanceCount * meshletsPerInstance;
        const u32 groupCountX = (totalThreads + 63) / 64;
        passCmd.dispatch(std::max(1u, groupCountX), 1, 1);
      });

  // Pass 3: Indirect Meshlet Visibility Rasterization (Graphics)
  renderGraph.addPass(
      "VisibilityPass", RGPassType::Graphics,
      [&](RenderPassBuilder &builder) {
        builder.read(indirectCmdTarget, PipelineStageFlags::DrawIndirect,
                     AccessFlags::IndirectCommandRead);
        builder.read(culledMeshletTarget,
                     PipelineStageFlags::TaskShader |
                         PipelineStageFlags::MeshShader,
                     AccessFlags::ShaderRead);
        builder.addColorAttachment(
            visTarget,
            ColorClearValue{.r = 0.0f, .g = 0.0f, .b = 0.0f, .a = 0.0f}, true);
        builder.setDepthAttachment(
            depthTarget, DepthStencilClearValue{.depth = 0.0f, .stencil = 0},
            true);
      },
      [&](CommandBuffer &passCmd) {
        passCmd.bindPipeline(*pipelines.visPipeline);
        passCmd.bindDescriptorSet(*frame.visDescSet, 0);

        struct VisibilityPushConstants {
          Mat4 viewProj;
          u32 isIndirect;
          u32 meshletOffset;
          u32 instanceId;
          u32 padding;
        } pc{.viewProj = viewProj,
             .isIndirect = 1,
             .meshletOffset = 0,
             .instanceId = 0,
             .padding = 0};

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

        // Dispatches mesh tasks indirectly from GPU compute culling buffer
        passCmd.drawMeshTasksIndirect(*frame.indirectDrawCommandBuffer, 0, 1,
                                      sizeof(u32) * 4);
      });

  // Pass 4: Deferred Texturing Resolve Pass (Graphics)
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
        passCmd.bindPipeline(*pipelines.deferredPipeline);

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
          Vec4 cameraPos;
          u32 screenWidth;
          u32 screenHeight;
          u32 debugMode;
          u32 instanceCount;
        } pc{.viewProj = viewProj,
             .cameraPos = Vec4(cameraPos, 1.0f),
             .screenWidth = curWidth,
             .screenHeight = curHeight,
             .debugMode = debugMode,
             .instanceCount = instanceCount};

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

  if (frame.readbackCountBuffer) {
    cmd.copyBuffer(*frame.indirectDrawCommandBuffer, *frame.readbackCountBuffer,
                   sizeof(u32) * 4, 0, 0);
  }

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
                     PipelinePack &pipelines, GpuScene &gpuScene, World &world,
                     std::vector<FrameResources> &frames, u32 maxFrames) {
  tests::common::Timer timer;
  tests::common::FlyCamController camera;
  camera.position = Vec3(0.0f, 15.0f, 45.0f);
  camera.pitch = -0.25f;

  f32 accumulator = 0.0f;
  u32 frameCounter = 0;
  u32 currentFrameIndex = 0;
  u32 totalFrames = 0;
  u32 currentDebugMode =
      1; // 1=meshlet, 2=triangle, 3=normals, 4=wireframe, 5=instance
  bool enableConeCull = true;

  RenderGraph renderGraph;

  ENGINE_LOG_INFO(
      "Fly-cam active. Controls: WASD/QE to move, RMB+drag to look.");
  ENGINE_LOG_INFO("Keys 1-5: Toggle debug modes (1:Meshlet, 2:Triangle, "
                  "3:Normals, 4:Wireframe, 5:Instance).");
  ENGINE_LOG_INFO("Key C: Toggle normal cone backface culling.");

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
      ENGINE_LOG_INFO("Debug Mode 1: Per-Meshlet Colors");
    } else if (input.isKeyPressed(KeyCode::Num2)) {
      currentDebugMode = 2;
      ENGINE_LOG_INFO("Debug Mode 2: Per-Triangle Colors");
    } else if (input.isKeyPressed(KeyCode::Num3)) {
      currentDebugMode = 3;
      ENGINE_LOG_INFO("Debug Mode 3: Surface Normals");
    } else if (input.isKeyPressed(KeyCode::Num4)) {
      currentDebugMode = 4;
      ENGINE_LOG_INFO("Debug Mode 4: Meshlet Wireframe");
    } else if (input.isKeyPressed(KeyCode::Num5)) {
      currentDebugMode = 5;
      ENGINE_LOG_INFO("Debug Mode 5: Per-Instance Colors");
    }

    if (input.isKeyPressed(KeyCode::C)) {
      enableConeCull = !enableConeCull;
      ENGINE_LOG_INFO("Normal cone backface culling: {}",
                      enableConeCull ? "ENABLED" : "DISABLED");
    }

    camera.update(input, deltaTime);

    // Update ECS animations
    updateEcsSystems(world, deltaTime);

    // Extract scene representations and dirty state from ECS
    gpuScene.extractFromEcs(world);

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

    auto &frame = frames[currentFrameIndex];
    if (!renderSceneFrame(window, device, swapchain, frame, pipelines, gpuScene,
                          viewProj, camera.position, currentDebugMode,
                          enableConeCull, renderGraph)) {
      break;
    }

    if (accumulator >= 1.0f) {
      const f32 fps = static_cast<f32>(frameCounter) / accumulator;
      const f32 frameTimeMs =
          (accumulator / static_cast<f32>(frameCounter)) * 1000.0f;
      const u32 totalMeshletTasks =
          gpuScene.getInstanceCount() *
          (gpuScene.getRegisteredMeshes().empty()
               ? 0
               : gpuScene.getRegisteredMeshes()[0].meshletCount);
      const u32 surviving = frames[currentFrameIndex].lastSurvivingMeshlets;
      const f32 cullPct =
          totalMeshletTasks > 0
              ? (1.0f - static_cast<f32>(surviving) /
                            static_cast<f32>(totalMeshletTasks)) *
                    100.0f
              : 0.0f;
      ENGINE_LOG_INFO(
          "FPS: {:.1f} | Frame Time: {:.3f} ms | Instances: {} | Surviving "
          "Meshlets: {} / {} (Culled: {:.1f}%) | Dirty Delta: {} | Mode: {} | "
          "ConeCull: {}",
          fps, frameTimeMs, gpuScene.getInstanceCount(), surviving,
          totalMeshletTasks, cullPct, gpuScene.getDirtyCount(),
          currentDebugMode, enableConeCull ? "ON" : "OFF");
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
  ENGINE_LOG_INFO("=== Milestone 5 Scenario: GPU Scene + Culling Pipeline ===");

  u32 maxFrames = 0;
  u32 windowWidth = 1280;
  u32 windowHeight = 720;
  u32 instanceCount = 1000;
  std::string modelPath;

  for (int i = 1; i < argc; ++i) {
    const std::string_view arg(argv[i]);
    if (arg == "--frames" && i + 1 < argc) {
      maxFrames = static_cast<u32>(std::atoi(argv[++i]));
    } else if (arg == "--width" && i + 1 < argc) {
      windowWidth = static_cast<u32>(std::atoi(argv[++i]));
    } else if (arg == "--height" && i + 1 < argc) {
      windowHeight = static_cast<u32>(std::atoi(argv[++i]));
    } else if (arg == "--instances" && i + 1 < argc) {
      instanceCount = static_cast<u32>(std::atoi(argv[++i]));
    } else if (arg == "--model" && i + 1 < argc) {
      modelPath = argv[++i];
    }
  }

  if (!Platform::init()) {
    ENGINE_LOG_FATAL("Failed to initialize engine platform.");
    return EXIT_FAILURE;
  }

  {
    WindowDesc winDesc{.title = "M5: GPU Scene + Culling Pipeline",
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

    ENGINE_LOG_INFO(
        "GPU: {} (Mesh Shaders, Compute & Indirect Dispatch active)",
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
      ENGINE_LOG_INFO("No model specified. Generating procedural clustered "
                      "torus knot...");
      meshAsset = createProceduralKnot();
    }

    ENGINE_LOG_INFO("Mesh statistics: {} vertices, {} meshlets, {} triangles",
                    meshAsset.getVertexCount(), meshAsset.getMeshletCount(),
                    meshAsset.getTriangleCount());

    World world;
    GpuScene gpuScene;

    const auto regMesh = gpuScene.registerMesh(*device, meshAsset);

    populateEcsScene(world, meshAsset, regMesh, instanceCount);

    gpuScene.extractFromEcs(world);

    const u32 totalSceneMeshletTasks = instanceCount * regMesh.meshletCount;

    auto frameResources = createFrameResources(*device, gpuScene, instanceCount,
                                               totalSceneMeshletTasks);

    auto pipelines = createPipelines(
        *device, swapchain->getFormat(), *frameResources[0].cullResetDescSet,
        *frameResources[0].instanceCullDescSet,
        *frameResources[0].meshletCullDescSet, *frameResources[0].visDescSet,
        *frameResources[0].deferredDescSet);

    ENGINE_LOG_INFO(
        "Entering scenario frame loop across {} frames in flight...",
        maxFramesInFlight);
    runScenarioLoop(window, *device, *swapchain, pipelines, gpuScene, world,
                    frameResources, maxFrames);
  }

  Platform::shutdown();
  ENGINE_LOG_INFO("=== Milestone 5 Scenario completed successfully ===");
  return EXIT_SUCCESS;
}
