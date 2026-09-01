#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>

#include <engine/core/mat4.hpp>
#include <engine/core/types.hpp>
#include <engine/core/vec3.hpp>
#include <engine/platform/input.hpp>
#include <engine/platform/key_codes.hpp>

namespace tests::common {

namespace core = engine::core;

class FlyCamController {
public:
  core::Vec3 position{0.0f, 1.0f, 3.5f};
  core::f32 yaw{0.0f};   // radians
  core::f32 pitch{0.0f}; // radians

  core::f32 moveSpeed{3.0f};
  core::f32 mouseSensitivity{0.003f};
  core::f32 fovY{60.0f * (std::numbers::pi_v<core::f32> / 180.0f)};
  core::f32 zNear{0.1f};
  core::f32 zFar{1000.0f};

  FlyCamController() = default;

  [[nodiscard]] core::Vec3 getForward() const noexcept {
    const core::f32 cp = std::cos(pitch);
    return core::Vec3(std::sin(yaw) * cp, std::sin(pitch), -std::cos(yaw) * cp)
        .normalized();
  }

  [[nodiscard]] core::Vec3 getRight() const noexcept {
    return getForward().cross(core::Vec3(0.0f, 1.0f, 0.0f)).normalized();
  }

  [[nodiscard]] core::Vec3 getUp() const noexcept {
    return getRight().cross(getForward()).normalized();
  }

  [[nodiscard]] core::Mat4 getViewMatrix() const noexcept {
    return core::Mat4::lookAt(position, position + getForward(),
                              core::Vec3(0.0f, 1.0f, 0.0f));
  }

  [[nodiscard]] core::Mat4
  getProjectionMatrix(core::f32 aspect) const noexcept {
    assert(aspect > 0.0f && fovY > 0.0f && zNear > 0.0f && zFar > zNear);
    const core::f32 tanHalfFov = std::tan(fovY * 0.5f);
    core::Mat4 m = core::Mat4::zeros();
    m.columns[0].x = 1.0f / (aspect * tanHalfFov);
    m.columns[1].y = -1.0f / tanHalfFov; // Negated for Vulkan NDC (+Y is down)
    m.columns[2].z = zNear / (zNear - zFar);
    m.columns[2].w = -1.0f;
    m.columns[3].z = (zNear * zFar) / (zFar - zNear);
    return m;
  }

  void update(const engine::platform::InputState &input,
              core::f32 deltaTime) noexcept {
    if (input.isMouseButtonDown(engine::platform::MouseButton::Right)) {
      const auto mouseDelta = input.getMouseDelta();
      yaw += mouseDelta.x * mouseSensitivity;
      pitch -= mouseDelta.y * mouseSensitivity;

      constexpr core::f32 maxPitch =
          89.0f * (std::numbers::pi_v<core::f32> / 180.0f);
      pitch = std::clamp(pitch, -maxPitch, maxPitch);
    }

    core::f32 speed = moveSpeed;
    if (input.isKeyDown(engine::platform::KeyCode::LeftShift)) {
      speed *= 2.5f;
    }

    const core::Vec3 forward = getForward();
    const core::Vec3 right = getRight();
    const core::Vec3 worldUp(0.0f, 1.0f, 0.0f);

    if (input.isKeyDown(engine::platform::KeyCode::W)) {
      position += forward * (speed * deltaTime);
    }
    if (input.isKeyDown(engine::platform::KeyCode::S)) {
      position -= forward * (speed * deltaTime);
    }
    if (input.isKeyDown(engine::platform::KeyCode::D)) {
      position += right * (speed * deltaTime);
    }
    if (input.isKeyDown(engine::platform::KeyCode::A)) {
      position -= right * (speed * deltaTime);
    }
    if (input.isKeyDown(engine::platform::KeyCode::E) ||
        input.isKeyDown(engine::platform::KeyCode::Space)) {
      position += worldUp * (speed * deltaTime);
    }
    if (input.isKeyDown(engine::platform::KeyCode::Q) ||
        input.isKeyDown(engine::platform::KeyCode::LeftControl)) {
      position -= worldUp * (speed * deltaTime);
    }
  }
};

} // namespace tests::common
