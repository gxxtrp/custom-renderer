#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include <engine/assets/asset_registry.hpp>
#include <engine/assets/gltf_loader.hpp>
#include <engine/assets/material_asset.hpp>
#include <engine/assets/mesh_asset.hpp>
#include <engine/assets/texture_asset.hpp>
#include <engine/core/log.hpp>
#include <engine/core/math.hpp>
#include <engine/core/profiler.hpp>
#include <engine/core/types.hpp>
#include <engine/ecs/world.hpp>
#include <engine/platform/input.hpp>
#include <engine/platform/key_codes.hpp>
#include <engine/platform/platform.hpp>
#include <engine/platform/window.hpp>
#include <engine/renderer/render_graph.hpp>
#include <engine/rhi/buffer.hpp>
#include <engine/rhi/command_buffer.hpp>
#include <engine/rhi/descriptor_set.hpp>
#include <engine/rhi/device.hpp>
#include <engine/rhi/pipeline.hpp>
#include <engine/rhi/sampler.hpp>
#include <engine/rhi/swapchain.hpp>
#include <engine/rhi/sync.hpp>
#include <engine/rhi/texture.hpp>
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
  fp.planes[2] = row3 + row1; // Top
  fp.planes[3] = row3 - row1; // Bottom
  fp.planes[4] = row3 - row2; // Near
  fp.planes[5] = row2;        // Far

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

std::vector<u8> loadSpirvFile(const std::filesystem::path &path) {
  std::ifstream file(path, std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    ENGINE_LOG_FATAL("Failed to open shader file: {}", path.string());
  }
  const auto fileSize = static_cast<usize>(file.tellg());
  std::vector<u8> buffer(fileSize);
  file.seekg(0);
  file.read(reinterpret_cast<char *>(buffer.data()),
            static_cast<std::streamsize>(fileSize));
  return buffer;
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
  std::unique_ptr<DescriptorSet> deferredSet0;
};

struct PipelinePack {
  std::unique_ptr<Pipeline> cullResetPipeline;
  std::unique_ptr<Pipeline> instanceCullPipeline;
  std::unique_ptr<Pipeline> meshletCullPipeline;
  std::unique_ptr<Pipeline> visPipeline;
  std::unique_ptr<Pipeline> deferredPipeline;

  std::unique_ptr<Sampler> materialSampler;
  std::unique_ptr<DescriptorSet> bindlessTextureSet;
};

// Shader-exact push constant structures
struct alignas(16) InstanceCullPushConstants {
  Vec4 frustumPlanes[6];
  Vec4 cameraPos;
  u32 instanceCount{0};
  u32 padding{0};
};

struct alignas(16) MeshletCullPushConstants {
  Vec4 frustumPlanes[6];
  Vec4 cameraPos;
  u32 instanceCount{0};
  u32 meshletsPerInstance{0};
  u32 enableConeCull{1};
  u32 padding{0};
};

struct alignas(16) VisibilityPushConstants {
  Mat4 viewProj;
  u32 isIndirect{1};
  u32 meshletOffset{0};
  u32 instanceId{0};
  u32 padding{0};
};

struct alignas(16) DeferredPushConstants {
  Mat4 viewProj;
  Vec4 cameraPos;
  Vec4 sunDirection; // xyz = dir towards sun, w = intensity
  Vec4 sunColor;     // xyz = light color, w = ambient intensity
  u32 screenWidth{1920};
  u32 screenHeight{1080};
  u32 debugMode{0};
  u32 instanceCount{0};
};

// Texture upload helper
std::unique_ptr<Texture> uploadTexture(Device &device,
                                       const TextureAsset &asset) {
  const TextureDesc texDesc{.width = asset.getWidth(),
                            .height = asset.getHeight(),
                            .depth = 1,
                            .mipLevels = 1,
                            .arrayLayers = 1,
                            .format = asset.getFormat(),
                            .usage = TextureUsageFlags::Sampled |
                                     TextureUsageFlags::TransferDst,
                            .dimension = TextureDimension::Texture2D};

  auto texRes = device.createTexture(texDesc);
  if (!texRes) {
    ENGINE_LOG_FATAL("Failed to create texture: {}", texRes.error());
  }
  auto texture = std::move(texRes.value());

  const usize byteSize = asset.getDataSize();
  const BufferDesc stagingDesc{.size = byteSize,
                               .usage = BufferUsageFlags::TransferSrc,
                               .memoryUsage = MemoryUsage::CpuToGpu};
  auto stagingRes = device.createBuffer(stagingDesc);
  if (!stagingRes) {
    ENGINE_LOG_FATAL("Failed to create texture staging buffer: {}",
                     stagingRes.error());
  }
  auto staging = std::move(stagingRes.value());
  auto mapRes = staging->map();
  if (mapRes) {
    std::memcpy(mapRes.value(), asset.getData().data(), byteSize);
    staging->unmap();
  }

  // Upload immediately via one-shot command buffer
  auto cmdRes = device.createCommandBuffer({});
  if (cmdRes) {
    auto cmd = std::move(cmdRes.value());
    cmd->begin();

    const ImageBarrier toDst{.texture = texture.get(),
                             .srcAccess = AccessFlags::None,
                             .dstAccess = AccessFlags::TransferWrite,
                             .srcStage = PipelineStageFlags::TopOfPipe,
                             .dstStage = PipelineStageFlags::Transfer,
                             .oldLayout = ImageLayout::Undefined,
                             .newLayout = ImageLayout::TransferDstOptimal};
    cmd->pipelineBarrier({.imageBarriers = {&toDst, 1}});

    const BufferTextureCopy copyRegion{
        .bufferOffset = 0,
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .mipLevel = 0,
        .baseArrayLayer = 0,
        .layerCount = 1,
        .imageOffset = {0, 0, 0},
        .imageExtent = {asset.getWidth(), asset.getHeight(), 1}};
    cmd->copyBufferToTexture(*staging, *texture, {&copyRegion, 1});

    const ImageBarrier toShaderRead{
        .texture = texture.get(),
        .srcAccess = AccessFlags::TransferWrite,
        .dstAccess = AccessFlags::ShaderRead,
        .srcStage = PipelineStageFlags::Transfer,
        .dstStage = PipelineStageFlags::FragmentShader |
                    PipelineStageFlags::ComputeShader,
        .oldLayout = ImageLayout::TransferDstOptimal,
        .newLayout = ImageLayout::ShaderReadOnlyOptimal};
    cmd->pipelineBarrier({.imageBarriers = {&toShaderRead, 1}});

    cmd->end();

    auto fenceRes = device.createFence({.signaled = false});
    if (fenceRes) {
      auto fence = std::move(fenceRes.value());
      CommandBuffer *cmdPtr = cmd.get();
      device.submit({.commandBuffer = cmdPtr, .signalFence = fence.get()});
      fence->wait();
    }
  }

  return texture;
}

// Procedural Pattern Texture Generator
TextureAsset createProceduralCheckerTexture(u32 width, u32 height) {
  std::vector<u8> pixels(width * height * 4);
  for (u32 y = 0; y < height; ++y) {
    for (u32 x = 0; x < width; ++x) {
      const u32 cx = (x / 16) % 2;
      const u32 cy = (y / 16) % 2;
      const u8 val = (cx ^ cy) ? 230 : 60;
      const usize idx = (y * width + x) * 4;
      pixels[idx + 0] = val;
      pixels[idx + 1] = val;
      pixels[idx + 2] = val;
      pixels[idx + 3] = 255;
    }
  }
  return TextureAsset(width, height, 4, Format::RGBA8_SRGB, std::move(pixels));
}

TextureAsset createProceduralNormalTexture(u32 width, u32 height) {
  std::vector<u8> pixels(width * height * 4);
  for (u32 y = 0; y < height; ++y) {
    for (u32 x = 0; x < width; ++x) {
      const f32 fx = static_cast<f32>(x) / static_cast<f32>(width) * 16.0f;
      const f32 fy = static_cast<f32>(y) / static_cast<f32>(height) * 16.0f;
      const f32 nx = std::sin(fx) * 0.4f;
      const f32 ny = std::cos(fy) * 0.4f;
      const f32 nz = std::sqrt(std::max(0.0f, 1.0f - nx * nx - ny * ny));

      const usize idx = (y * width + x) * 4;
      pixels[idx + 0] = static_cast<u8>((nx * 0.5f + 0.5f) * 255.0f);
      pixels[idx + 1] = static_cast<u8>((ny * 0.5f + 0.5f) * 255.0f);
      pixels[idx + 2] = static_cast<u8>((nz * 0.5f + 0.5f) * 255.0f);
      pixels[idx + 3] = 255;
    }
  }
  return TextureAsset(width, height, 4, Format::RGBA8_UNORM, std::move(pixels));
}

} // namespace

int main(int argc, char **argv) {
  ENGINE_LOG_INFO("=== Milestone 6 Scenario: Materials + PBR Shading ===");

  auto platformRes = Platform::init();
  if (!platformRes) {
    ENGINE_LOG_FATAL("Platform init failed: {}", platformRes.error());
    return 1;
  }

  constexpr u32 initialWidth = 1920;
  constexpr u32 initialHeight = 1080;

  WindowDesc windowDesc{
      .title = "Milestone 6: Materials + PBR Shading (Cook-Torrance GGX)",
      .width = initialWidth,
      .height = initialHeight,
      .resizable = true,
      .highDpi = true};

  auto windowRes = Window::create(windowDesc);
  if (!windowRes) {
    ENGINE_LOG_FATAL("Window creation failed: {}", windowRes.error());
    return 1;
  }
  Window window = std::move(windowRes.value());

  const DeviceDesc deviceDesc{.enableValidation = true,
                              .preferDiscreteGpu = true};
  auto deviceRes = engine::rhi_vulkan::VulkanDevice::create(deviceDesc);
  if (!deviceRes) {
    ENGINE_LOG_FATAL("Vulkan device creation failed: {}", deviceRes.error());
    return 1;
  }
  auto device = std::move(deviceRes.value());

  const SwapchainDesc swapchainDesc{.windowHandle = window.getNativeHandle(),
                                    .width = window.getWidth(),
                                    .height = window.getHeight(),
                                    .imageCount = maxFramesInFlight,
                                    .preferredFormat = Format::BGRA8_UNORM,
                                    .preferredPresentMode =
                                        PresentMode::Immediate};

  auto swapchainRes = device->createSwapchain(swapchainDesc);
  if (!swapchainRes) {
    ENGINE_LOG_FATAL("Swapchain creation failed: {}", swapchainRes.error());
    return 1;
  }
  auto swapchain = std::move(swapchainRes.value());

  // 1. Initialize Asset Registry and Procedural Assets
  AssetRegistry assetRegistry;

  // Add extra procedural textures
  const auto checkerTexIdx =
      assetRegistry.registerTexture(createProceduralCheckerTexture(256, 256));
  const auto waveNormalTexIdx =
      assetRegistry.registerTexture(createProceduralNormalTexture(256, 256));

  // Register Materials
  // Mat 1: Gold Metal
  MaterialAsset goldMat{};
  goldMat.baseColorFactor = Vec4(1.000f, 0.782f, 0.344f, 1.0f);
  goldMat.metallicFactor = 1.0f;
  goldMat.roughnessFactor = 0.12f;
  goldMat.baseColorTextureIndex = AssetRegistry::DefaultWhiteTextureIndex;
  goldMat.normalTextureIndex = AssetRegistry::DefaultNormalTextureIndex;
  goldMat.metallicRoughnessTextureIndex =
      AssetRegistry::DefaultMetallicRoughnessTextureIndex;
  goldMat.emissiveTextureIndex = AssetRegistry::DefaultBlackTextureIndex;
  const auto goldMatIdx = assetRegistry.registerMaterial(goldMat);

  // Mat 2: Copper / Patterned Normal
  MaterialAsset copperMat{};
  copperMat.baseColorFactor = Vec4(0.955f, 0.637f, 0.538f, 1.0f);
  copperMat.metallicFactor = 1.0f;
  copperMat.roughnessFactor = 0.20f;
  copperMat.normalScale = 1.0f;
  copperMat.baseColorTextureIndex = AssetRegistry::DefaultWhiteTextureIndex;
  copperMat.normalTextureIndex = waveNormalTexIdx;
  copperMat.metallicRoughnessTextureIndex =
      AssetRegistry::DefaultMetallicRoughnessTextureIndex;
  copperMat.emissiveTextureIndex = AssetRegistry::DefaultBlackTextureIndex;
  const auto copperMatIdx = assetRegistry.registerMaterial(copperMat);

  // Mat 3: Red Plastic / Dielectric
  MaterialAsset redPlasticMat{};
  redPlasticMat.baseColorFactor = Vec4(0.85f, 0.1f, 0.1f, 1.0f);
  redPlasticMat.metallicFactor = 0.0f;
  redPlasticMat.roughnessFactor = 0.1f;
  redPlasticMat.baseColorTextureIndex = AssetRegistry::DefaultWhiteTextureIndex;
  redPlasticMat.normalTextureIndex = AssetRegistry::DefaultNormalTextureIndex;
  redPlasticMat.metallicRoughnessTextureIndex =
      AssetRegistry::DefaultMetallicRoughnessTextureIndex;
  redPlasticMat.emissiveTextureIndex = AssetRegistry::DefaultBlackTextureIndex;
  const auto redPlasticMatIdx = assetRegistry.registerMaterial(redPlasticMat);

  // Mat 4: Checkerboard Coated Surface
  MaterialAsset checkerMat{};
  checkerMat.baseColorFactor = Vec4(1.0f, 1.0f, 1.0f, 1.0f);
  checkerMat.metallicFactor = 0.1f;
  checkerMat.roughnessFactor = 0.4f;
  checkerMat.baseColorTextureIndex = checkerTexIdx;
  checkerMat.normalTextureIndex = waveNormalTexIdx;
  checkerMat.metallicRoughnessTextureIndex =
      AssetRegistry::DefaultMetallicRoughnessTextureIndex;
  checkerMat.emissiveTextureIndex = AssetRegistry::DefaultBlackTextureIndex;
  const auto checkerMatIdx = assetRegistry.registerMaterial(checkerMat);

  // Mat 5: Emerald Dielectric
  MaterialAsset emeraldMat{};
  emeraldMat.baseColorFactor = Vec4(0.1f, 0.85f, 0.35f, 1.0f);
  emeraldMat.metallicFactor = 0.0f;
  emeraldMat.roughnessFactor = 0.2f;
  emeraldMat.baseColorTextureIndex = AssetRegistry::DefaultWhiteTextureIndex;
  emeraldMat.normalTextureIndex = AssetRegistry::DefaultNormalTextureIndex;
  emeraldMat.metallicRoughnessTextureIndex =
      AssetRegistry::DefaultMetallicRoughnessTextureIndex;
  emeraldMat.emissiveTextureIndex = AssetRegistry::DefaultBlackTextureIndex;
  const auto emeraldMatIdx = assetRegistry.registerMaterial(emeraldMat);

  // 2. Upload all registry textures to GPU
  std::vector<std::unique_ptr<Texture>> gpuTextures;
  for (const auto &texAsset : assetRegistry.getTextures()) {
    gpuTextures.push_back(uploadTexture(*device, texAsset));
  }

  // 3. Create Sampler
  const SamplerDesc samplerDesc{.minFilter = Filter::Linear,
                                .magFilter = Filter::Linear,
                                .mipmapMode = SamplerMipmapMode::Linear,
                                .addressModeU = SamplerAddressMode::Repeat,
                                .addressModeV = SamplerAddressMode::Repeat,
                                .addressModeW = SamplerAddressMode::Repeat,
                                .maxAnisotropy = 16.0f,
                                .compareEnable = false,
                                .minLod = 0.0f,
                                .maxLod = 16.0f};
  auto samplerRes = device->createSampler(samplerDesc);
  if (!samplerRes) {
    ENGINE_LOG_FATAL("Failed to create material sampler: {}",
                     samplerRes.error());
    return 1;
  }
  auto materialSampler = std::move(samplerRes.value());

  // 4. Build Meshes and World Scene
  GpuScene gpuScene;

  // Register all materials into GpuScene
  for (const auto &mat : assetRegistry.getMaterials()) {
    (void)gpuScene.registerMaterial(GpuMaterialData::fromMaterialAsset(mat));
  }

  World world;

  // Check if a glTF scene was supplied on the command line
  bool loadedCustomGltf = false;
  if (argc > 1) {
    const std::filesystem::path gltfPath(argv[1]);
    if (std::filesystem::exists(gltfPath)) {
      ENGINE_LOG_INFO("Loading glTF scene from: {}", gltfPath.string());
      auto gltfRes = loadGltfScene(gltfPath);
      if (gltfRes) {
        auto &sceneAsset = gltfRes.value();

        // Upload custom glTF textures and offset indices
        const u32 textureBaseIdx = static_cast<u32>(gpuTextures.size());
        for (const auto &tex : sceneAsset.textures) {
          gpuTextures.push_back(uploadTexture(*device, tex));
        }

        // Register custom glTF materials
        const u32 materialBaseIdx = gpuScene.getMaterialCount();
        for (auto &mat : sceneAsset.materials) {
          if (mat.baseColorTextureIndex > 0)
            mat.baseColorTextureIndex += textureBaseIdx;
          if (mat.normalTextureIndex > 0)
            mat.normalTextureIndex += textureBaseIdx;
          if (mat.metallicRoughnessTextureIndex > 0)
            mat.metallicRoughnessTextureIndex += textureBaseIdx;
          if (mat.emissiveTextureIndex > 0)
            mat.emissiveTextureIndex += textureBaseIdx;
          if (mat.occlusionTextureIndex > 0)
            mat.occlusionTextureIndex += textureBaseIdx;

          (void)gpuScene.registerMaterial(
              GpuMaterialData::fromMaterialAsset(mat));
        }

        // Register meshes
        std::vector<RegisteredMesh> registeredGltfMeshes;
        for (const auto &mesh : sceneAsset.meshes) {
          registeredGltfMeshes.push_back(gpuScene.registerMesh(*device, mesh));
        }

        // Spawn instances in ECS
        for (const auto &inst : sceneAsset.instances) {
          if (inst.meshIndex < registeredGltfMeshes.size()) {
            const auto &reg = registeredGltfMeshes[inst.meshIndex];
            const auto &meshAsset = sceneAsset.meshes[inst.meshIndex];

            auto entity = world.createEntity();
            Transform t;
            t.position = Vec3(inst.transform(0, 3), inst.transform(1, 3),
                              inst.transform(2, 3));
            t.dirty = true;
            world.addComponent(entity, std::move(t));

            MeshComponent mc(&meshAsset, reg.meshId, reg.meshletOffset);
            mc.materialId = inst.materialIndex + materialBaseIdx;
            world.addComponent(entity, std::move(mc));
          }
        }
        loadedCustomGltf = true;
      }
    }
  }

  if (!loadedCustomGltf) {
    ENGINE_LOG_INFO(
        "Building procedural showcase scene with diverse PBR materials...");
    const MeshAsset knotAsset = createProceduralKnot(128, 32);
    const RegisteredMesh regKnot = gpuScene.registerMesh(*device, knotAsset);

    // Grid of PBR Torus Knots
    const struct {
      Vec3 pos;
      u32 matId;
      f32 spinSpeed;
    } knots[] = {
        {Vec3(-4.0f, 0.0f, 0.0f), goldMatIdx, 0.8f},
        {Vec3(-2.0f, 0.0f, 0.0f), copperMatIdx, -0.6f},
        {Vec3(0.0f, 0.0f, 0.0f), checkerMatIdx, 0.4f},
        {Vec3(2.0f, 0.0f, 0.0f), emeraldMatIdx, -0.7f},
        {Vec3(4.0f, 0.0f, 0.0f), redPlasticMatIdx, 0.5f},
        {Vec3(-3.0f, 2.5f, 1.0f), goldMatIdx, 0.6f},
        {Vec3(-1.0f, 2.5f, 1.0f), redPlasticMatIdx, -0.5f},
        {Vec3(1.0f, 2.5f, 1.0f), copperMatIdx, 0.7f},
        {Vec3(3.0f, 2.5f, 1.0f), emeraldMatIdx, -0.4f},
        {Vec3(-3.0f, -2.5f, 1.0f), checkerMatIdx, -0.5f},
        {Vec3(-1.0f, -2.5f, 1.0f), emeraldMatIdx, 0.6f},
        {Vec3(1.0f, -2.5f, 1.0f), goldMatIdx, -0.8f},
        {Vec3(3.0f, -2.5f, 1.0f), redPlasticMatIdx, 0.6f},
    };

    for (const auto &item : knots) {
      auto entity = world.createEntity();
      Transform t;
      t.position = item.pos;
      t.scale = Vec3(0.9f, 0.9f, 0.9f);
      t.dirty = true;
      world.addComponent(entity, std::move(t));

      MeshComponent mc(&knotAsset, regKnot.meshId, regKnot.meshletOffset);
      mc.materialId = item.matId;
      world.addComponent(entity, std::move(mc));

      world.addComponent(entity, SpinAnimation{.axis = Vec3(0.0f, 1.0f, 0.0f),
                                               .speed = item.spinSpeed});
    }
  }

  // 5. Build Pipelines and Descriptor Layouts
  PipelinePack pipelines;
  pipelines.materialSampler = std::move(materialSampler);

  // Allocate per-frame sync and culling resources
  FrameResources frames[maxFramesInFlight];
  constexpr usize maxInstances = 2048;
  constexpr usize maxMeshlets = 65536;

  for (auto &frame : frames) {
    frame.commandBuffer = device->createCommandBuffer({}).value();
    frame.imageAvailableSemaphore = device->createSemaphore({}).value();
    frame.renderFinishedSemaphore = device->createSemaphore({}).value();
    frame.inFlightFence = device->createFence({.signaled = true}).value();

    frame.visibleInstanceIndicesBuffer =
        device
            ->createBuffer({.size = maxInstances * sizeof(u32),
                            .usage = BufferUsageFlags::StorageBuffer,
                            .memoryUsage = MemoryUsage::GpuOnly})
            .value();
    frame.visibleInstanceCountBuffer =
        device
            ->createBuffer({.size = sizeof(u32),
                            .usage = BufferUsageFlags::StorageBuffer |
                                     BufferUsageFlags::TransferDst,
                            .memoryUsage = MemoryUsage::GpuOnly})
            .value();
    frame.indirectDrawCommandBuffer =
        device
            ->createBuffer({.size = 16,
                            .usage = BufferUsageFlags::StorageBuffer |
                                     BufferUsageFlags::Indirect |
                                     BufferUsageFlags::TransferDst,
                            .memoryUsage = MemoryUsage::GpuOnly})
            .value();
    frame.culledMeshletListBuffer =
        device
            ->createBuffer({.size = maxMeshlets * sizeof(u32) * 2,
                            .usage = BufferUsageFlags::StorageBuffer,
                            .memoryUsage = MemoryUsage::GpuOnly})
            .value();
    frame.culledMeshletCountBuffer =
        device
            ->createBuffer({.size = 16,
                            .usage = BufferUsageFlags::StorageBuffer |
                                     BufferUsageFlags::TransferDst |
                                     BufferUsageFlags::TransferSrc,
                            .memoryUsage = MemoryUsage::GpuOnly})
            .value();
    frame.readbackCountBuffer =
        device
            ->createBuffer({.size = 16,
                            .usage = BufferUsageFlags::TransferDst,
                            .memoryUsage = MemoryUsage::GpuToCpu})
            .value();

    // 0. Cull Reset Descriptor Set (3 bindings: 0..2)
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
    frame.cullResetDescSet =
        device->createDescriptorSet({.bindings = resetBindings}).value();
    frame.cullResetDescSet->updateBuffer(
        0, {.buffer = frame.visibleInstanceCountBuffer.get()});
    frame.cullResetDescSet->updateBuffer(
        1, {.buffer = frame.indirectDrawCommandBuffer.get()});
    frame.cullResetDescSet->updateBuffer(
        2, {.buffer = frame.culledMeshletCountBuffer.get()});

    // 1. Instance Cull Descriptor Set (4 bindings: 0..3)
    const DescriptorBindingDesc instanceCullBindings[] = {
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
    frame.instanceCullDescSet =
        device->createDescriptorSet({.bindings = instanceCullBindings}).value();
    frame.instanceCullDescSet->updateBuffer(
        0, {.buffer = gpuScene.getInstanceBuffer()});
    frame.instanceCullDescSet->updateBuffer(
        1, {.buffer = frame.visibleInstanceIndicesBuffer.get()});
    frame.instanceCullDescSet->updateBuffer(
        2, {.buffer = frame.visibleInstanceCountBuffer.get()});
    frame.instanceCullDescSet->updateBuffer(
        3, {.buffer = frame.indirectDrawCommandBuffer.get()});

    // 2. Meshlet Cull Descriptor Set (7 bindings: 0..6 matching
    // meshlet_cull.slang)
    const DescriptorBindingDesc meshletCullBindings[] = {
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
    frame.meshletCullDescSet =
        device->createDescriptorSet({.bindings = meshletCullBindings}).value();
    frame.meshletCullDescSet->updateBuffer(
        0, {.buffer = gpuScene.getInstanceBuffer()});
    frame.meshletCullDescSet->updateBuffer(
        1, {.buffer = gpuScene.getMeshletBuffer()});
    frame.meshletCullDescSet->updateBuffer(
        2, {.buffer = frame.visibleInstanceIndicesBuffer.get()});
    frame.meshletCullDescSet->updateBuffer(
        3, {.buffer = frame.visibleInstanceCountBuffer.get()});
    frame.meshletCullDescSet->updateBuffer(
        4, {.buffer = frame.indirectDrawCommandBuffer.get()});
    frame.meshletCullDescSet->updateBuffer(
        5, {.buffer = frame.culledMeshletListBuffer.get()});
    frame.meshletCullDescSet->updateBuffer(
        6, {.buffer = frame.culledMeshletCountBuffer.get()});

    // 3. Visibility Descriptor Set (6 bindings: 0..5 matching
    // meshlet_vis_gpu_scene.slang)
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
    frame.visDescSet =
        device->createDescriptorSet({.bindings = visBindings}).value();
    frame.visDescSet->updateBuffer(0, {.buffer = gpuScene.getInstanceBuffer()});
    frame.visDescSet->updateBuffer(1, {.buffer = gpuScene.getMeshletBuffer()});
    frame.visDescSet->updateBuffer(2, {.buffer = gpuScene.getVertexBuffer()});
    frame.visDescSet->updateBuffer(
        3, {.buffer = gpuScene.getMeshletVertexBuffer()});
    frame.visDescSet->updateBuffer(
        4, {.buffer = gpuScene.getMeshletTriangleBuffer()});
    frame.visDescSet->updateBuffer(
        5, {.buffer = frame.culledMeshletListBuffer.get()});

    // 4. Deferred Set 0 (6 bindings: 0..5 with UpdateAfterBind)
    const DescriptorBindingDesc deferredSet0Bindings[] = {
        {.stageFlags = PipelineStageFlags::FragmentShader,
         .binding = 0,
         .count = 1,
         .type = DescriptorType::SampledTexture,
         .flags = DescriptorBindingFlags::PartiallyBound |
                  DescriptorBindingFlags::UpdateAfterBind},
        {.stageFlags = PipelineStageFlags::FragmentShader,
         .binding = 1,
         .count = 1,
         .type = DescriptorType::SampledTexture,
         .flags = DescriptorBindingFlags::PartiallyBound |
                  DescriptorBindingFlags::UpdateAfterBind},
        {.stageFlags = PipelineStageFlags::FragmentShader,
         .binding = 2,
         .count = 1,
         .type = DescriptorType::StorageBuffer,
         .flags = DescriptorBindingFlags::PartiallyBound |
                  DescriptorBindingFlags::UpdateAfterBind},
        {.stageFlags = PipelineStageFlags::FragmentShader,
         .binding = 3,
         .count = 1,
         .type = DescriptorType::StorageBuffer,
         .flags = DescriptorBindingFlags::PartiallyBound |
                  DescriptorBindingFlags::UpdateAfterBind},
        {.stageFlags = PipelineStageFlags::FragmentShader,
         .binding = 4,
         .count = 1,
         .type = DescriptorType::StorageBuffer,
         .flags = DescriptorBindingFlags::PartiallyBound |
                  DescriptorBindingFlags::UpdateAfterBind},
        {.stageFlags = PipelineStageFlags::FragmentShader,
         .binding = 5,
         .count = 1,
         .type = DescriptorType::StorageBuffer,
         .flags = DescriptorBindingFlags::PartiallyBound |
                  DescriptorBindingFlags::UpdateAfterBind},
    };
    frame.deferredSet0 =
        device->createDescriptorSet({.bindings = deferredSet0Bindings}).value();
    frame.deferredSet0->updateBuffer(2, {.buffer = gpuScene.getVertexBuffer()});
    frame.deferredSet0->updateBuffer(
        3, {.buffer = gpuScene.getGlobalIndexBuffer()});
    frame.deferredSet0->updateBuffer(4,
                                     {.buffer = gpuScene.getInstanceBuffer()});
    frame.deferredSet0->updateBuffer(5,
                                     {.buffer = gpuScene.getMaterialBuffer()});
  }

  // Bindless Set (Set 1 of Deferred pipeline)
  const u32 maxBindlessTextures =
      std::max(128u, static_cast<u32>(gpuTextures.size() + 16));
  const DescriptorBindingDesc deferredBindlessBindings[] = {
      {.stageFlags = PipelineStageFlags::FragmentShader,
       .binding = 0,
       .count = 1,
       .type = DescriptorType::Sampler,
       .flags = DescriptorBindingFlags::None},
      {.stageFlags = PipelineStageFlags::FragmentShader,
       .binding = 1,
       .count = maxBindlessTextures,
       .type = DescriptorType::SampledTexture,
       .flags = DescriptorBindingFlags::PartiallyBound |
                DescriptorBindingFlags::UpdateAfterBind},
  };
  auto bindlessSetRes =
      device->createDescriptorSet({.bindings = deferredBindlessBindings});
  if (!bindlessSetRes) {
    ENGINE_LOG_FATAL("Failed to create bindless descriptor set: {}",
                     bindlessSetRes.error());
    return 1;
  }
  pipelines.bindlessTextureSet = std::move(bindlessSetRes.value());

  // Update Sampler at Binding 0
  pipelines.bindlessTextureSet->updateSampler(
      0, {.sampler = pipelines.materialSampler.get()});

  // Update Textures at Binding 1
  std::vector<TextureBindingInfo> texBindings;
  texBindings.reserve(gpuTextures.size());
  for (const auto &tex : gpuTextures) {
    texBindings.push_back(TextureBindingInfo{
        .texture = tex.get(), .layout = ImageLayout::ShaderReadOnlyOptimal});
  }
  pipelines.bindlessTextureSet->updateTextures(1, 0, texBindings);

  // Load SPIR-V and Create Pipelines
  const auto cullResetSpv =
      loadSpirvFile(resolveShaderPath("cull_reset.comp.spv"));
  const auto instCullSpv =
      loadSpirvFile(resolveShaderPath("instance_cull.comp.spv"));
  const auto meshletCullSpv =
      loadSpirvFile(resolveShaderPath("meshlet_cull.comp.spv"));
  const auto visTaskSpv =
      loadSpirvFile(resolveShaderPath("meshlet_vis_gpu_scene.task.spv"));
  const auto visMeshSpv =
      loadSpirvFile(resolveShaderPath("meshlet_vis_gpu_scene.mesh.spv"));
  const auto visFragSpv =
      loadSpirvFile(resolveShaderPath("meshlet_vis_gpu_scene.frag.spv"));
  const auto pbrMeshSpv =
      loadSpirvFile(resolveShaderPath("pbr_deferred.mesh.spv"));
  const auto pbrFragSpv =
      loadSpirvFile(resolveShaderPath("pbr_deferred.frag.spv"));

  // 1. Cull Reset Pipeline
  {
    const DescriptorSet *sets[] = {frames[0].cullResetDescSet.get()};
    const PipelineDesc desc{.computeShaderCode = cullResetSpv,
                            .bindPoint = PipelineBindPoint::Compute,
                            .descriptorSets = sets};
    pipelines.cullResetPipeline = device->createPipeline(desc).value();
  }

  // 2. Instance Cull Pipeline
  {
    const DescriptorSet *sets[] = {frames[0].instanceCullDescSet.get()};
    const PipelineDesc desc{.computeShaderCode = instCullSpv,
                            .bindPoint = PipelineBindPoint::Compute,
                            .descriptorSets = sets};
    pipelines.instanceCullPipeline = device->createPipeline(desc).value();
  }

  // 3. Meshlet Cull Pipeline
  {
    const DescriptorSet *sets[] = {frames[0].meshletCullDescSet.get()};
    const PipelineDesc desc{.computeShaderCode = meshletCullSpv,
                            .bindPoint = PipelineBindPoint::Compute,
                            .descriptorSets = sets};
    pipelines.meshletCullPipeline = device->createPipeline(desc).value();
  }

  // 4. Visibility Pipeline
  {
    const Format colorFormats[] = {Format::RG32_UINT};
    const DescriptorSet *sets[] = {frames[0].visDescSet.get()};
    const PipelineDesc desc{
        .taskShaderCode = visTaskSpv,
        .meshShaderCode = visMeshSpv,
        .fragmentShaderCode = visFragSpv,
        .colorAttachmentFormats = colorFormats,
        .rasterizer = {.polygonMode = PolygonMode::Fill,
                       .cullMode = CullMode::None,
                       .frontFace = FrontFace::CounterClockwise},
        .depthStencil = {.depthCompareOp = CompareOp::GreaterOrEqual,
                         .depthTestEnable = true,
                         .depthWriteEnable = true},
        .depthAttachmentFormat = Format::D32_FLOAT,
        .bindPoint = PipelineBindPoint::Graphics,
        .descriptorSets = sets};
    pipelines.visPipeline = device->createPipeline(desc).value();
  }

  // 5. PBR Deferred Pipeline (Two Descriptor Sets: Set 0 and Set 1)
  {
    const Format colorFormats[] = {Format::BGRA8_UNORM};
    const DescriptorSet *sets[] = {frames[0].deferredSet0.get(),
                                   pipelines.bindlessTextureSet.get()};
    const PipelineDesc desc{
        .meshShaderCode = pbrMeshSpv,
        .fragmentShaderCode = pbrFragSpv,
        .colorAttachmentFormats = colorFormats,
        .rasterizer = {.polygonMode = PolygonMode::Fill,
                       .cullMode = CullMode::None},
        .depthStencil = {.depthTestEnable = false, .depthWriteEnable = false},
        .depthAttachmentFormat = Format::Undefined,
        .bindPoint = PipelineBindPoint::Graphics,
        .descriptorSets = sets};
    pipelines.deferredPipeline = device->createPipeline(desc).value();
  }

  ENGINE_LOG_INFO("All PBR and culling pipelines created successfully!");

  // 6. Interactive Camera & Lighting Setup
  tests::common::FlyCamController camera;
  camera.position = Vec3(0.0f, 1.5f, 9.0f);

  f32 sunAzimuth = 0.8f;
  f32 sunElevation = 0.9f;
  f32 sunIntensity = 4.0f;
  f32 ambientIntensity = 0.35f;

  u32 debugMode =
      0; // 0: PBR Lit, 1: Meshlets, 2: Triangles, 3: Normals, 4: Wireframe, 5:
         // Instances, 6: Albedo, 7: Roughness/Metallic
  bool enableConeCull = true;
  bool isPaused = false;

  tests::common::Timer timer;
  u32 currentFrameIndex = 0;
  RenderGraph renderGraph;

  ENGINE_LOG_INFO("Entering interactive PBR render loop...");
  ENGINE_LOG_INFO("Controls:");
  ENGINE_LOG_INFO("  WASD / QE + Right-Click Drag: Fly Camera");
  ENGINE_LOG_INFO("  Arrow Keys: Orbit Sun azimuth & elevation");
  ENGINE_LOG_INFO("  U / O: Adjust Sun intensity");
  ENGINE_LOG_INFO("  0..7: Switch PBR / Debug visualizations");
  ENGINE_LOG_INFO("  C: Toggle Cone Culling");
  ENGINE_LOG_INFO("  Space: Pause / Resume rotation");

  while (!window.shouldClose()) {
    ENGINE_PROFILE_FRAME();
    const f32 dt = timer.tick();

    // Pump window events
    Platform::pollEvents();
    const auto &input = Platform::getInput();

    if (input.isKeyPressed(KeyCode::Escape)) {
      window.setShouldClose(true);
      break;
    }

    if (input.isKeyPressed(KeyCode::Space)) {
      isPaused = !isPaused;
    }
    if (input.isKeyPressed(KeyCode::C)) {
      enableConeCull = !enableConeCull;
      ENGINE_LOG_INFO("Cone Culling: {}", enableConeCull ? "ON" : "OFF");
    }

    // Debug Mode keys 0..7
    if (input.isKeyPressed(KeyCode::Num0))
      debugMode = 0;
    if (input.isKeyPressed(KeyCode::Num1))
      debugMode = 1;
    if (input.isKeyPressed(KeyCode::Num2))
      debugMode = 2;
    if (input.isKeyPressed(KeyCode::Num3))
      debugMode = 3;
    if (input.isKeyPressed(KeyCode::Num4))
      debugMode = 4;
    if (input.isKeyPressed(KeyCode::Num5))
      debugMode = 5;
    if (input.isKeyPressed(KeyCode::Num6))
      debugMode = 6;
    if (input.isKeyPressed(KeyCode::Num7))
      debugMode = 7;

    // Sun light orbit
    if (input.isKeyDown(KeyCode::Left))
      sunAzimuth -= dt * 1.5f;
    if (input.isKeyDown(KeyCode::Right))
      sunAzimuth += dt * 1.5f;
    if (input.isKeyDown(KeyCode::Up))
      sunElevation = std::min(1.55f, sunElevation + dt * 1.2f);
    if (input.isKeyDown(KeyCode::Down))
      sunElevation = std::max(0.05f, sunElevation - dt * 1.2f);
    if (input.isKeyDown(KeyCode::U))
      sunIntensity = std::max(0.0f, sunIntensity - dt * 2.0f);
    if (input.isKeyDown(KeyCode::O))
      sunIntensity += dt * 2.0f;

    // Camera movement
    camera.update(input, dt);

    // Animate knots
    if (!isPaused) {
      auto spinQuery = world.view<Transform, SpinAnimation>();
      spinQuery.each([&](Entity, Transform &t, const SpinAnimation &spin) {
        t.rotation =
            (Quat::fromAxisAngle(spin.axis, spin.speed * dt) * t.rotation)
                .normalized();
        t.dirty = true;
      });
    }

    // Extract ECS to GpuScene
    gpuScene.extractFromEcs(world);

    const u32 curWidth = window.getWidth();
    const u32 curHeight = window.getHeight();
    if (curWidth == 0 || curHeight == 0) {
      continue;
    }

    if (curWidth != swapchain->getWidth() ||
        curHeight != swapchain->getHeight()) {
      auto resizeRes = swapchain->resize(curWidth, curHeight);
      if (!resizeRes) {
        ENGINE_LOG_ERROR("Failed to resize swapchain: {}", resizeRes.error());
      }
    }

    auto &frame = frames[currentFrameIndex];
    frame.inFlightFence->wait();
    frame.inFlightFence->reset();

    // Acquire Swapchain Image
    auto acquireRes =
        swapchain->acquireNextImage(*frame.imageAvailableSemaphore);
    if (!acquireRes) {
      continue;
    }
    const u32 imageIndex = acquireRes.value().imageIndex;

    const f32 aspect = static_cast<f32>(curWidth) / static_cast<f32>(curHeight);
    const Mat4 view = camera.getViewMatrix();
    const Mat4 proj = camera.getProjectionMatrix(aspect);
    const Mat4 viewProj = proj * view;
    const FrustumPlanes frustum = extractFrustumPlanes(viewProj);
    const u32 instanceCount = gpuScene.getInstanceCount();
    const u32 meshletsPerInstance =
        gpuScene.getRegisteredMeshes().empty()
            ? 0
            : gpuScene.getRegisteredMeshes()[0].meshletCount;

    const Vec3 sunDir = Vec3(std::cos(sunElevation) * std::sin(sunAzimuth),
                             std::sin(sunElevation),
                             std::cos(sunElevation) * std::cos(sunAzimuth))
                            .normalized();

    // Record Commands
    auto &cmd = *frame.commandBuffer;
    cmd.begin();

    // 1. Upload GPU Scene Delta
    gpuScene.uploadDelta(*device, cmd);

    // 2. Update descriptor sets with valid GPU scene buffer handles in case
    // reallocated
    frame.instanceCullDescSet->updateBuffer(
        0, {.buffer = gpuScene.getInstanceBuffer()});
    frame.meshletCullDescSet->updateBuffer(
        0, {.buffer = gpuScene.getInstanceBuffer()});
    frame.visDescSet->updateBuffer(0, {.buffer = gpuScene.getInstanceBuffer()});
    frame.deferredSet0->updateBuffer(4,
                                     {.buffer = gpuScene.getInstanceBuffer()});
    frame.deferredSet0->updateBuffer(5,
                                     {.buffer = gpuScene.getMaterialBuffer()});

    // Reset RenderGraph for this frame
    renderGraph.reset();

    Texture &backBuffer = swapchain->getImage(imageIndex);
    auto swapTarget = renderGraph.importTexture(
        "SwapchainBackbuffer", &backBuffer, ImageLayout::Undefined,
        ImageLayout::PresentSrc);

    auto visTarget = renderGraph.createTexture(
        RGTextureDesc{.name = "VisibilityBuffer",
                      .width = swapchain->getWidth(),
                      .height = swapchain->getHeight(),
                      .depth = 1,
                      .mipLevels = 1,
                      .arrayLayers = 1,
                      .format = Format::RG32_UINT,
                      .usage = TextureUsageFlags::ColorAttachment |
                               TextureUsageFlags::Sampled,
                      .dimension = TextureDimension::Texture2D});

    auto depthTarget = renderGraph.createTexture(
        RGTextureDesc{.name = "SceneDepth",
                      .width = swapchain->getWidth(),
                      .height = swapchain->getHeight(),
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
          builder.read(indirectCmdTarget, PipelineStageFlags::ComputeShader,
                       AccessFlags::ShaderRead | AccessFlags::ShaderWrite);
        },
        [&](CommandBuffer &passCmd) {
          passCmd.bindPipeline(*pipelines.instanceCullPipeline);
          passCmd.bindDescriptorSet(*frame.instanceCullDescSet, 0);

          const InstanceCullPushConstants pc{
              .frustumPlanes = {frustum.planes[0], frustum.planes[1],
                                frustum.planes[2], frustum.planes[3],
                                frustum.planes[4], frustum.planes[5]},
              .cameraPos = Vec4(camera.position, 1.0f),
              .instanceCount = instanceCount,
              .padding = 0};

          passCmd.pushConstants(PipelineStageFlags::ComputeShader, 0,
                                sizeof(pc), &pc);

          const u32 groupCountX = (instanceCount + 63) / 64;
          passCmd.dispatch(std::max(1u, groupCountX), 1, 1);
        });

    // Pass 2: Meshlet Frustum and Normal Cone Culling (Compute)
    renderGraph.addPass(
        "MeshletCullPass", RGPassType::Compute,
        [&](RenderPassBuilder &builder) {
          builder.read(indirectCmdTarget, PipelineStageFlags::ComputeShader,
                       AccessFlags::ShaderRead | AccessFlags::ShaderWrite);
          builder.write(culledMeshletTarget, PipelineStageFlags::ComputeShader,
                        AccessFlags::ShaderWrite);
        },
        [&](CommandBuffer &passCmd) {
          passCmd.bindPipeline(*pipelines.meshletCullPipeline);
          passCmd.bindDescriptorSet(*frame.meshletCullDescSet, 0);

          const MeshletCullPushConstants pc{
              .frustumPlanes = {frustum.planes[0], frustum.planes[1],
                                frustum.planes[2], frustum.planes[3],
                                frustum.planes[4], frustum.planes[5]},
              .cameraPos = Vec4(camera.position, 1.0f),
              .instanceCount = instanceCount,
              .meshletsPerInstance = meshletsPerInstance,
              .enableConeCull = enableConeCull ? 1u : 0u,
              .padding = 0};

          passCmd.pushConstants(PipelineStageFlags::ComputeShader, 0,
                                sizeof(pc), &pc);

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
              ColorClearValue{.r = 0.0f, .g = 0.0f, .b = 0.0f, .a = 0.0f},
              true);
          builder.setDepthAttachment(
              depthTarget, DepthStencilClearValue{.depth = 0.0f, .stencil = 0},
              true);
        },
        [&](CommandBuffer &passCmd) {
          passCmd.bindPipeline(*pipelines.visPipeline);
          passCmd.bindDescriptorSet(*frame.visDescSet, 0);

          const VisibilityPushConstants pc{.viewProj = viewProj,
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

          passCmd.drawMeshTasksIndirect(*frame.indirectDrawCommandBuffer, 0, 1,
                                        sizeof(u32) * 4);
        });

    // Pass 4: PBR Deferred Shading Resolve Pass (Graphics)
    renderGraph.addPass(
        "PbrDeferredShadingPass", RGPassType::Graphics,
        [&](RenderPassBuilder &builder) {
          builder.read(visTarget, PipelineStageFlags::FragmentShader,
                       AccessFlags::ShaderRead,
                       ImageLayout::ShaderReadOnlyOptimal);
          builder.read(depthTarget, PipelineStageFlags::FragmentShader,
                       AccessFlags::ShaderRead,
                       ImageLayout::DepthStencilReadOnlyOptimal);
          builder.addColorAttachment(
              swapTarget,
              ColorClearValue{.r = 0.02f, .g = 0.03f, .b = 0.05f, .a = 1.0f},
              false);
        },
        [&](CommandBuffer &passCmd) {
          passCmd.bindPipeline(*pipelines.deferredPipeline);

          auto *physVis = renderGraph.getPhysicalTexture(visTarget);
          auto *physDepth = renderGraph.getPhysicalTexture(depthTarget);

          frame.deferredSet0->updateTexture(
              0, {.texture = physVis,
                  .layout = ImageLayout::ShaderReadOnlyOptimal});
          frame.deferredSet0->updateTexture(
              1, {.texture = physDepth,
                  .layout = ImageLayout::DepthStencilReadOnlyOptimal});

          passCmd.bindDescriptorSet(*frame.deferredSet0, 0);
          passCmd.bindDescriptorSet(*pipelines.bindlessTextureSet, 1);

          const DeferredPushConstants dpc{
              .viewProj = viewProj,
              .cameraPos = Vec4(camera.position, 1.0f),
              .sunDirection = Vec4(sunDir.x, sunDir.y, sunDir.z, sunIntensity),
              .sunColor = Vec4(1.0f, 0.98f, 0.92f, ambientIntensity),
              .screenWidth = curWidth,
              .screenHeight = curHeight,
              .debugMode = debugMode,
              .instanceCount = instanceCount,
          };
          passCmd.pushConstants(PipelineStageFlags::FragmentShader, 0,
                                sizeof(dpc), &dpc);

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

    renderGraph.compile(*device);
    renderGraph.execute(*device, cmd);

    cmd.end();

    // Submit Queue
    const Semaphore *waitSems[] = {frame.imageAvailableSemaphore.get()};
    const PipelineStageFlags waitStages[] = {
        PipelineStageFlags::ColorAttachmentOutput};
    const Semaphore *signalSems[] = {frame.renderFinishedSemaphore.get()};

    const SubmitInfo submitInfo{.commandBuffer = &cmd,
                                .signalFence = frame.inFlightFence.get(),
                                .waitSemaphores = waitSems,
                                .waitStages = waitStages,
                                .signalSemaphores = signalSems};
    device->submit(submitInfo);

    // Present Swapchain
    auto presentRes =
        swapchain->present(imageIndex, *frame.renderFinishedSemaphore);
    if (!presentRes) {
      ENGINE_LOG_ERROR("Present error: {}", presentRes.error());
    }

    currentFrameIndex = (currentFrameIndex + 1) % maxFramesInFlight;
  }

  // Clean device shutdown
  device->waitIdle();
  ENGINE_LOG_INFO("M6 PBR Lit Scene shutdown cleanly!");

  return 0;
}
