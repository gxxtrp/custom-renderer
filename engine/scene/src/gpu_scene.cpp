#include <engine/scene/gpu_scene.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>

#include <engine/core/log.hpp>
#include <engine/core/profiler.hpp>

namespace engine::scene {

namespace {

std::unique_ptr<rhi::Buffer>
createAndUploadBuffer(rhi::Device &device, const void *data, core::usize size,
                      rhi::BufferUsageFlags usage, std::string_view /*name*/) {
  if (size == 0) {
    return nullptr;
  }
  const rhi::BufferDesc desc{.size = size,
                             .usage = usage |
                                      rhi::BufferUsageFlags::StorageBuffer |
                                      rhi::BufferUsageFlags::TransferDst,
                             .memoryUsage = rhi::MemoryUsage::CpuToGpu};

  auto bufRes = device.createBuffer(desc);
  if (!bufRes) {
    ENGINE_LOG_ERROR("Failed to create GPU scene buffer: {}", bufRes.error());
    return nullptr;
  }
  auto buf = std::move(bufRes.value());
  auto mapRes = buf->map();
  if (mapRes) {
    std::memcpy(mapRes.value(), data, size);
    buf->unmap();
  }
  return buf;
}

} // namespace

GpuScene::GpuScene() {
  m_instances.reserve(2048);
  m_dirtyInstanceIndices.reserve(2048);
  m_isDirty.reserve(2048);
  m_registeredMeshes.reserve(64);
}

GpuScene::~GpuScene() = default;

RegisteredMesh GpuScene::registerMesh(rhi::Device &device,
                                      const assets::MeshAsset &meshAsset) {
  RegisteredMesh reg{};
  reg.meshId = static_cast<core::u32>(m_registeredMeshes.size());

  reg.meshletOffset = static_cast<core::u32>(m_globalMeshlets.size());
  reg.meshletCount = static_cast<core::u32>(meshAsset.getMeshletCount());

  reg.vertexOffset = static_cast<core::u32>(m_globalVertices.size());
  reg.vertexCount = static_cast<core::u32>(meshAsset.getVertexCount());

  reg.meshletVertexOffset =
      static_cast<core::u32>(m_globalMeshletVertices.size());
  reg.meshletVertexCount =
      static_cast<core::u32>(meshAsset.getMeshletVertices().size());

  reg.meshletTriangleOffset =
      static_cast<core::u32>(m_globalMeshletTriangles.size());
  reg.meshletTriangleCount =
      static_cast<core::u32>(meshAsset.getMeshletTriangles().size());

  reg.globalIndexOffset = static_cast<core::u32>(m_globalIndices.size());
  reg.globalIndexCount =
      static_cast<core::u32>(meshAsset.getGlobalIndices().size());

  // Append meshlets with adjusted offsets
  for (const auto &m : meshAsset.getMeshlets()) {
    assets::Meshlet adjusted = m;
    adjusted.meshletId += reg.meshletOffset;
    adjusted.vertexOffset += reg.meshletVertexOffset;
    adjusted.primitiveOffset += reg.meshletTriangleOffset;
    m_globalMeshlets.push_back(adjusted);
  }

  // Append vertices
  const auto verts = meshAsset.getVertices();
  m_globalVertices.insert(m_globalVertices.end(), verts.begin(), verts.end());

  // Append meshlet vertices (indices pointing to local vertices + vertexOffset)
  for (auto idx : meshAsset.getMeshletVertices()) {
    m_globalMeshletVertices.push_back(idx + reg.vertexOffset);
  }

  // Append meshlet triangles
  const auto tris = meshAsset.getMeshletTriangles();
  m_globalMeshletTriangles.insert(m_globalMeshletTriangles.end(), tris.begin(),
                                  tris.end());

  // Append global indices
  for (auto idx : meshAsset.getGlobalIndices()) {
    m_globalIndices.push_back(idx + reg.vertexOffset);
  }

  m_totalMeshletCount = static_cast<core::u32>(m_globalMeshlets.size());
  m_registeredMeshes.push_back(reg);

  // Upload/re-upload global geometry buffers to GPU
  m_meshletBuffer = createAndUploadBuffer(
      device, m_globalMeshlets.data(),
      m_globalMeshlets.size() * sizeof(assets::Meshlet),
      rhi::BufferUsageFlags::StorageBuffer, "GlobalMeshlets");

  m_vertexBuffer = createAndUploadBuffer(
      device, m_globalVertices.data(),
      m_globalVertices.size() * sizeof(assets::Vertex),
      rhi::BufferUsageFlags::StorageBuffer, "GlobalVertices");

  m_meshletVertexBuffer = createAndUploadBuffer(
      device, m_globalMeshletVertices.data(),
      m_globalMeshletVertices.size() * sizeof(core::u32),
      rhi::BufferUsageFlags::StorageBuffer, "GlobalMeshletVertices");

  m_meshletTriangleBuffer = createAndUploadBuffer(
      device, m_globalMeshletTriangles.data(),
      m_globalMeshletTriangles.size() * sizeof(core::u32),
      rhi::BufferUsageFlags::StorageBuffer, "GlobalMeshletTriangles");

  m_globalIndexBuffer = createAndUploadBuffer(
      device, m_globalIndices.data(),
      m_globalIndices.size() * sizeof(core::u32),
      rhi::BufferUsageFlags::StorageBuffer, "GlobalIndices");

  ENGINE_LOG_INFO(
      "Registered mesh {}: {} meshlets (global total: {}), {} vertices",
      reg.meshId, reg.meshletCount, m_totalMeshletCount, reg.vertexCount);

  return reg;
}

void GpuScene::extractFromEcs(ecs::World &world) {
  ENGINE_PROFILE_ZONE_NAMED("GpuScene::ExtractFromEcs");

  auto query = world.view<Transform, MeshComponent>();
  const core::usize totalCount = query.count();

  if (m_instances.size() != totalCount) {
    m_instances.resize(totalCount);
    m_isDirty.assign(totalCount, true);
    m_dirtyInstanceIndices.clear();
    m_dirtyInstanceIndices.reserve(totalCount);
    for (core::u32 i = 0; i < totalCount; ++i) {
      m_dirtyInstanceIndices.push_back(i);
    }
    m_fullReuploadRequired = true;
  }

  core::u32 instanceIndex = 0;
  query.each([&](ecs::Entity /*entity*/, Transform &transform,
                 const MeshComponent &meshComp) {
    if (transform.dirty || m_fullReuploadRequired || m_isDirty[instanceIndex]) {
      const core::Mat4 modelMatrix = transform.computeModelMatrix();
      const core::Mat4 normalMatrix = transform.computeNormalMatrix();

      // Transform local bounding sphere to world space
      const core::Vec4 localCenter4(meshComp.localBoundingSphereCenter, 1.0f);
      const core::Vec4 worldCenter4 = modelMatrix * localCenter4;
      const core::Vec3 worldCenter(worldCenter4.x, worldCenter4.y,
                                   worldCenter4.z);

      const core::f32 maxScale = std::max(
          std::abs(transform.scale.x),
          std::max(std::abs(transform.scale.y), std::abs(transform.scale.z)));
      const core::f32 worldRadius =
          meshComp.localBoundingSphereRadius * maxScale;

      // Compute tight world AABB taking full rotation & scale into account
      const core::Vec3 localBoxCenter =
          (meshComp.localAabbMin + meshComp.localAabbMax) * 0.5f;
      const core::Vec3 localBoxExtents =
          (meshComp.localAabbMax - meshComp.localAabbMin) * 0.5f;

      const core::Vec4 worldBoxCenter4 =
          modelMatrix * core::Vec4(localBoxCenter, 1.0f);
      const core::Vec3 worldBoxCenter(worldBoxCenter4.x, worldBoxCenter4.y,
                                      worldBoxCenter4.z);

      const core::Vec3 worldBoxExtents(
          std::abs(modelMatrix(0, 0)) * localBoxExtents.x +
              std::abs(modelMatrix(0, 1)) * localBoxExtents.y +
              std::abs(modelMatrix(0, 2)) * localBoxExtents.z,
          std::abs(modelMatrix(1, 0)) * localBoxExtents.x +
              std::abs(modelMatrix(1, 1)) * localBoxExtents.y +
              std::abs(modelMatrix(1, 2)) * localBoxExtents.z,
          std::abs(modelMatrix(2, 0)) * localBoxExtents.x +
              std::abs(modelMatrix(2, 1)) * localBoxExtents.y +
              std::abs(modelMatrix(2, 2)) * localBoxExtents.z);

      const core::Vec3 worldAabbMin = worldBoxCenter - worldBoxExtents;
      const core::Vec3 worldAabbMax = worldBoxCenter + worldBoxExtents;

      auto &inst = m_instances[instanceIndex];
      inst.modelMatrix = modelMatrix;
      inst.normalMatrix = normalMatrix;
      inst.boundingSphere =
          core::Vec4(worldCenter.x, worldCenter.y, worldCenter.z, worldRadius);
      inst.aabbMin = core::Vec4(worldAabbMin.x, worldAabbMin.y, worldAabbMin.z,
                                static_cast<core::f32>(meshComp.meshletOffset));
      inst.aabbMax = core::Vec4(worldAabbMax.x, worldAabbMax.y, worldAabbMax.z,
                                static_cast<core::f32>(meshComp.meshletCount));
      inst.meshId = meshComp.meshId;
      inst.instanceId = instanceIndex;
      inst.materialId = meshComp.materialId;
      inst.flags = 0;

      if (!m_isDirty[instanceIndex]) {
        m_isDirty[instanceIndex] = true;
        m_dirtyInstanceIndices.push_back(instanceIndex);
      }

      transform.dirty = false;
    }
    ++instanceIndex;
  });
}

void GpuScene::ensureInstanceBuffers(rhi::Device &device,
                                     core::usize requiredCount) {
  if (requiredCount == 0) {
    return;
  }

  if (requiredCount > m_instanceBufferCapacity || m_instanceBuffer == nullptr) {
    m_instanceBufferCapacity = std::max(requiredCount, core::usize(1024));
    const core::usize bufferSize =
        m_instanceBufferCapacity * sizeof(GpuInstanceData);

    const rhi::BufferDesc gpuDesc{
        .size = bufferSize,
        .usage = rhi::BufferUsageFlags::StorageBuffer |
                 rhi::BufferUsageFlags::TransferDst |
                 rhi::BufferUsageFlags::ShaderDeviceAddress,
        .memoryUsage = rhi::MemoryUsage::GpuOnly};

    auto gpuBufRes = device.createBuffer(gpuDesc);
    if (!gpuBufRes) {
      ENGINE_LOG_FATAL("Failed to allocate GPU scene instance buffer: {}",
                       gpuBufRes.error());
    }
    m_instanceBuffer = std::move(gpuBufRes.value());

    const rhi::BufferDesc stagingDesc{
        .size = bufferSize,
        .usage = rhi::BufferUsageFlags::TransferSrc,
        .memoryUsage = rhi::MemoryUsage::CpuToGpu};

    auto stagingBufRes = device.createBuffer(stagingDesc);
    if (!stagingBufRes) {
      ENGINE_LOG_FATAL("Failed to allocate instance staging buffer: {}",
                       stagingBufRes.error());
    }
    m_instanceStagingBuffer = std::move(stagingBufRes.value());

    m_fullReuploadRequired = true;
  }
}

void GpuScene::uploadDelta(rhi::Device &device, rhi::CommandBuffer &cmd) {
  ENGINE_PROFILE_ZONE_NAMED("GpuScene::UploadDelta");

  if (m_instances.empty()) {
    return;
  }

  ensureInstanceBuffers(device, m_instances.size());

  bool didCopy = false;

  if (m_fullReuploadRequired) {
    auto mapRes = m_instanceStagingBuffer->map();
    if (!mapRes) {
      ENGINE_LOG_ERROR("Failed to map staging buffer for full upload");
      return;
    }
    const core::usize totalBytes = m_instances.size() * sizeof(GpuInstanceData);
    std::memcpy(mapRes.value(), m_instances.data(), totalBytes);
    m_instanceStagingBuffer->unmap();

    cmd.copyBuffer(*m_instanceStagingBuffer, *m_instanceBuffer, totalBytes, 0,
                   0);

    m_dirtyInstanceIndices.clear();
    m_isDirty.assign(m_instances.size(), false);
    m_fullReuploadRequired = false;
    didCopy = true;
  } else if (!m_dirtyInstanceIndices.empty()) {
    auto mapRes = m_instanceStagingBuffer->map();
    if (!mapRes) {
      ENGINE_LOG_ERROR("Failed to map staging buffer for delta upload");
      return;
    }

    auto *stagingPtr = static_cast<core::u8 *>(mapRes.value());

    // Pack sparse dirty updates into staging buffer
    for (core::u32 dirtyIdx : m_dirtyInstanceIndices) {
      const core::usize offset = dirtyIdx * sizeof(GpuInstanceData);
      std::memcpy(stagingPtr + offset, &m_instances[dirtyIdx],
                  sizeof(GpuInstanceData));
      cmd.copyBuffer(*m_instanceStagingBuffer, *m_instanceBuffer,
                     sizeof(GpuInstanceData), offset, offset);
      m_isDirty[dirtyIdx] = false;
    }

    m_instanceStagingBuffer->unmap();
    m_dirtyInstanceIndices.clear();
    didCopy = true;
  }

  if (didCopy) {
    const rhi::BufferBarrier copyBarrier{
        .buffer = m_instanceBuffer.get(),
        .offset = 0,
        .size = 0,
        .srcAccess = rhi::AccessFlags::TransferWrite,
        .dstAccess = rhi::AccessFlags::ShaderRead,
        .srcStage = rhi::PipelineStageFlags::Transfer,
        .dstStage = rhi::PipelineStageFlags::ComputeShader |
                    rhi::PipelineStageFlags::TaskShader |
                    rhi::PipelineStageFlags::MeshShader |
                    rhi::PipelineStageFlags::FragmentShader};
    const rhi::PipelineBarrierDesc barrierDesc{
        .bufferBarriers = {&copyBarrier, 1}};
    cmd.pipelineBarrier(barrierDesc);
  }
}

} // namespace engine::scene
