#include <engine/scene/transform.hpp>

namespace engine::scene {

core::Mat4 Transform::computeModelMatrix() const noexcept {
  return core::Mat4::translation(position) * rotation.toMat4() *
         core::Mat4::scaling(scale);
}

core::Mat4 Transform::computeNormalMatrix() const noexcept {
  const core::f32 invSx = (scale.x != 0.0f) ? (1.0f / scale.x) : 1.0f;
  const core::f32 invSy = (scale.y != 0.0f) ? (1.0f / scale.y) : 1.0f;
  const core::f32 invSz = (scale.z != 0.0f) ? (1.0f / scale.z) : 1.0f;
  return rotation.toMat4() *
         core::Mat4::scaling(core::Vec3(invSx, invSy, invSz));
}

} // namespace engine::scene
