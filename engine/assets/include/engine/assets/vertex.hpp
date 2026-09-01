#pragma once

#include <engine/core/types.hpp>
#include <engine/core/vec2.hpp>
#include <engine/core/vec3.hpp>

namespace engine::assets {

struct alignas(16) Vertex {
  core::Vec3 position{0.0f, 0.0f, 0.0f}; // 16 bytes (x, y, z, pad)
  core::Vec3 normal{0.0f, 1.0f, 0.0f};   // 16 bytes (x, y, z, pad)
  core::Vec2 uv{0.0f, 0.0f};             // 8 bytes (x, y)
  core::f32 _pad[2]{0.0f, 0.0f};         // 8 bytes pad to 48 bytes std430

  [[nodiscard]] constexpr bool
  operator==(const Vertex &) const noexcept = default;
};

static_assert(
    sizeof(Vertex) == 48,
    "Vertex must be 48 bytes for GPU storage buffer std430 alignment");

} // namespace engine::assets
