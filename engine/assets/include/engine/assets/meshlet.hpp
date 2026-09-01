#pragma once

#include <engine/core/types.hpp>
#include <engine/core/vec3.hpp>

namespace engine::assets {

struct alignas(16) Meshlet {
  core::f32 center[3]{0.0f, 0.0f, 0.0f};
  core::f32 radius{0.0f};

  core::f32 coneAxis[3]{0.0f, 0.0f, 0.0f};
  core::f32 coneCutoff{0.0f};

  core::f32 coneApex[3]{0.0f, 0.0f, 0.0f};
  core::u32 meshletId{0};

  core::u32 vertexOffset{0};
  core::u32 vertexCount{0};
  core::u32 primitiveOffset{0};
  core::u32 primitiveCount{0};

  [[nodiscard]] core::Vec3 getCenter() const noexcept {
    return core::Vec3(center[0], center[1], center[2]);
  }
  [[nodiscard]] core::Vec3 getConeAxis() const noexcept {
    return core::Vec3(coneAxis[0], coneAxis[1], coneAxis[2]);
  }
  [[nodiscard]] core::Vec3 getConeApex() const noexcept {
    return core::Vec3(coneApex[0], coneApex[1], coneApex[2]);
  }

  void setCenter(const core::Vec3 &c) noexcept {
    center[0] = c.x;
    center[1] = c.y;
    center[2] = c.z;
  }

  void setConeAxis(const core::Vec3 &a) noexcept {
    coneAxis[0] = a.x;
    coneAxis[1] = a.y;
    coneAxis[2] = a.z;
  }

  void setConeApex(const core::Vec3 &a) noexcept {
    coneApex[0] = a.x;
    coneApex[1] = a.y;
    coneApex[2] = a.z;
  }
};

static_assert(sizeof(Meshlet) == 64,
              "Meshlet descriptor must occupy exactly one 64-byte cache line");

} // namespace engine::assets
