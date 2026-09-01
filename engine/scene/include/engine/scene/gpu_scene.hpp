#pragma once

#include <memory>
#include <span>
#include <string>
#include <vector>

#include <engine/assets/mesh_asset.hpp>
#include <engine/core/types.hpp>
#include <engine/ecs/world.hpp>
#include <engine/rhi/buffer.hpp>
#include <engine/rhi/command_buffer.hpp>
#include <engine/rhi/device.hpp>
#include <engine/scene/mesh_component.hpp>
#include <engine/scene/scene_instance.hpp>
#include <engine/scene/transform.hpp>

namespace engine::scene {

struct RegisteredMesh {
  core::u32 meshId{0};
  core::u32 meshletOffset{0};
  core::u32 meshletCount{0};
  core::u32 vertexOffset{0};
  core::u32 vertexCount{0};
  core::u32 meshletVertexOffset{0};
  core::u32 meshletVertexCount{0};
  core::u32 meshletTriangleOffset{0};
  core::u32 meshletTriangleCount{0};
  core::u32 globalIndexOffset{0};
  core::u32 globalIndexCount{0};
};

class GpuScene {
public:
  GpuScene();
  ~GpuScene();

  GpuScene(const GpuScene &) = delete;
  GpuScene &operator=(const GpuScene &) = delete;

  GpuScene(GpuScene &&) noexcept = default;
  GpuScene &operator=(GpuScene &&) noexcept = default;

  [[nodiscard]] RegisteredMesh registerMesh(rhi::Device &device,
                                            const assets::MeshAsset &meshAsset);

  void extractFromEcs(ecs::World &world);

  void uploadDelta(rhi::Device &device, rhi::CommandBuffer &cmd);

  [[nodiscard]] rhi::Buffer *getInstanceBuffer() const noexcept {
    return m_instanceBuffer.get();
  }
  [[nodiscard]] rhi::Buffer *getMeshletBuffer() const noexcept {
    return m_meshletBuffer.get();
  }
  [[nodiscard]] rhi::Buffer *getVertexBuffer() const noexcept {
    return m_vertexBuffer.get();
  }
  [[nodiscard]] rhi::Buffer *getMeshletVertexBuffer() const noexcept {
    return m_meshletVertexBuffer.get();
  }
  [[nodiscard]] rhi::Buffer *getMeshletTriangleBuffer() const noexcept {
    return m_meshletTriangleBuffer.get();
  }
  [[nodiscard]] rhi::Buffer *getGlobalIndexBuffer() const noexcept {
    return m_globalIndexBuffer.get();
  }

  [[nodiscard]] core::u32 getInstanceCount() const noexcept {
    return static_cast<core::u32>(m_instances.size());
  }

  [[nodiscard]] core::u32 getTotalMeshletCount() const noexcept {
    return m_totalMeshletCount;
  }

  [[nodiscard]] core::u32 getDirtyCount() const noexcept {
    return static_cast<core::u32>(m_dirtyInstanceIndices.size());
  }

  [[nodiscard]] std::span<const GpuInstanceData> getInstances() const noexcept {
    return m_instances;
  }

  [[nodiscard]] std::span<const RegisteredMesh>
  getRegisteredMeshes() const noexcept {
    return m_registeredMeshes;
  }

private:
  void ensureInstanceBuffers(rhi::Device &device, core::usize requiredCount);

  std::vector<GpuInstanceData> m_instances;
  std::vector<core::u32> m_dirtyInstanceIndices;
  std::vector<bool> m_isDirty;

  std::vector<assets::Meshlet> m_globalMeshlets;
  std::vector<assets::Vertex> m_globalVertices;
  std::vector<core::u32> m_globalMeshletVertices;
  std::vector<core::u32> m_globalMeshletTriangles;
  std::vector<core::u32> m_globalIndices;
  std::vector<RegisteredMesh> m_registeredMeshes;

  std::unique_ptr<rhi::Buffer> m_instanceBuffer;
  std::unique_ptr<rhi::Buffer> m_instanceStagingBuffer;
  std::unique_ptr<rhi::Buffer> m_meshletBuffer;
  std::unique_ptr<rhi::Buffer> m_vertexBuffer;
  std::unique_ptr<rhi::Buffer> m_meshletVertexBuffer;
  std::unique_ptr<rhi::Buffer> m_meshletTriangleBuffer;
  std::unique_ptr<rhi::Buffer> m_globalIndexBuffer;

  core::usize m_instanceBufferCapacity{0};
  core::u32 m_totalMeshletCount{0};
  bool m_fullReuploadRequired{true};
};

} // namespace engine::scene
