#pragma once

#include <engine/core/mat4.hpp>
#include <engine/core/quat.hpp>
#include <engine/core/types.hpp>
#include <engine/core/vec3.hpp>

namespace engine::scene {

struct alignas(16) Transform {
  core::Vec3 position{0.0f, 0.0f, 0.0f};
  core::Quat rotation{core::Quat::identity()};
  core::Vec3 scale{1.0f, 1.0f, 1.0f};
  bool dirty{true};

  [[nodiscard]] core::Mat4 computeModelMatrix() const noexcept;
  [[nodiscard]] core::Mat4 computeNormalMatrix() const noexcept;
};

} // namespace engine::scene
