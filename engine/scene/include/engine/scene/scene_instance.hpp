#pragma once

#include <engine/core/mat4.hpp>
#include <engine/core/types.hpp>
#include <engine/core/vec3.hpp>
#include <engine/core/vec4.hpp>

namespace engine::scene {

struct alignas(16) GpuInstanceData {
  core::Mat4 modelMatrix{core::Mat4::identity()};
  core::Mat4 normalMatrix{core::Mat4::identity()};
  core::Vec4 boundingSphere{0.0f, 0.0f, 0.0f, 0.0f}; // xyz = center, w = radius
  core::Vec4 aabbMin{0.0f, 0.0f, 0.0f,
                     0.0f}; // xyz = min, w = float(meshletOffset)
  core::Vec4 aabbMax{0.0f, 0.0f, 0.0f,
                     0.0f}; // xyz = max, w = float(meshletCount)
  core::u32 meshId{0};
  core::u32 instanceId{0};
  core::u32 materialId{0};
  core::u32 flags{0};

  [[nodiscard]] core::Vec3 getCenter() const noexcept {
    return core::Vec3(boundingSphere.x, boundingSphere.y, boundingSphere.z);
  }
  [[nodiscard]] core::f32 getRadius() const noexcept {
    return boundingSphere.w;
  }
  [[nodiscard]] core::Vec3 getAabbMin() const noexcept {
    return core::Vec3(aabbMin.x, aabbMin.y, aabbMin.z);
  }
  [[nodiscard]] core::Vec3 getAabbMax() const noexcept {
    return core::Vec3(aabbMax.x, aabbMax.y, aabbMax.z);
  }
  [[nodiscard]] core::u32 getMeshletOffset() const noexcept {
    return static_cast<core::u32>(aabbMin.w);
  }
  [[nodiscard]] core::u32 getMeshletCount() const noexcept {
    return static_cast<core::u32>(aabbMax.w);
  }
};

static_assert(
    sizeof(GpuInstanceData) == 192,
    "GpuInstanceData layout must occupy exactly 192 bytes (3 cache lines)");

} // namespace engine::scene
