#pragma once

#include <array>

#include <engine/core/math.hpp>
#include <engine/core/types.hpp>
#include <engine/platform/key_codes.hpp>

namespace engine::platform {

class InputState {
public:
    InputState() noexcept;

    [[nodiscard]] bool isKeyDown(KeyCode key) const noexcept;
    [[nodiscard]] bool isKeyPressed(KeyCode key) const noexcept;
    [[nodiscard]] bool isKeyReleased(KeyCode key) const noexcept;

    [[nodiscard]] bool isMouseButtonDown(MouseButton button) const noexcept;
    [[nodiscard]] bool isMouseButtonPressed(MouseButton button) const noexcept;
    [[nodiscard]] bool isMouseButtonReleased(MouseButton button) const noexcept;

    [[nodiscard]] core::Vec2 getMousePosition() const noexcept { return m_mousePosition; }
    [[nodiscard]] core::Vec2 getMouseDelta() const noexcept { return m_mouseDelta; }
    [[nodiscard]] core::Vec2 getMouseScroll() const noexcept { return m_mouseScroll; }

    void newFrame() noexcept;

    void onKeyDown(KeyCode key) noexcept;
    void onKeyUp(KeyCode key) noexcept;
    void onMouseButtonDown(MouseButton button) noexcept;
    void onMouseButtonUp(MouseButton button) noexcept;
    void onMouseMove(core::f32 x, core::f32 y, core::f32 dx, core::f32 dy) noexcept;
    void onMouseScroll(core::f32 sx, core::f32 sy) noexcept;

private:
    static constexpr core::usize maxKeys = static_cast<core::usize>(KeyCode::Count);
    static constexpr core::usize maxButtons = static_cast<core::usize>(MouseButton::Count);

    std::array<bool, maxKeys> m_currentKeys{};
    std::array<bool, maxKeys> m_previousKeys{};

    std::array<bool, maxButtons> m_currentButtons{};
    std::array<bool, maxButtons> m_previousButtons{};

    core::Vec2 m_mousePosition{0.0f, 0.0f};
    core::Vec2 m_mouseDelta{0.0f, 0.0f};
    core::Vec2 m_mouseScroll{0.0f, 0.0f};
};

}  // namespace engine::platform
