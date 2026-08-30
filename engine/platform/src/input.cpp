#include <engine/platform/input.hpp>

namespace engine::platform {

InputState::InputState() noexcept {
    m_currentKeys.fill(false);
    m_previousKeys.fill(false);
    m_currentButtons.fill(false);
    m_previousButtons.fill(false);
}

bool InputState::isKeyDown(KeyCode key) const noexcept {
    const auto idx = static_cast<core::usize>(key);
    if (idx < maxKeys) {
        return m_currentKeys[idx];
    }
    return false;
}

bool InputState::isKeyPressed(KeyCode key) const noexcept {
    const auto idx = static_cast<core::usize>(key);
    if (idx < maxKeys) {
        return m_currentKeys[idx] && !m_previousKeys[idx];
    }
    return false;
}

bool InputState::isKeyReleased(KeyCode key) const noexcept {
    const auto idx = static_cast<core::usize>(key);
    if (idx < maxKeys) {
        return !m_currentKeys[idx] && m_previousKeys[idx];
    }
    return false;
}

bool InputState::isMouseButtonDown(MouseButton button) const noexcept {
    const auto idx = static_cast<core::usize>(button);
    if (idx < maxButtons) {
        return m_currentButtons[idx];
    }
    return false;
}

bool InputState::isMouseButtonPressed(MouseButton button) const noexcept {
    const auto idx = static_cast<core::usize>(button);
    if (idx < maxButtons) {
        return m_currentButtons[idx] && !m_previousButtons[idx];
    }
    return false;
}

bool InputState::isMouseButtonReleased(MouseButton button) const noexcept {
    const auto idx = static_cast<core::usize>(button);
    if (idx < maxButtons) {
        return !m_currentButtons[idx] && m_previousButtons[idx];
    }
    return false;
}

void InputState::newFrame() noexcept {
    m_previousKeys = m_currentKeys;
    m_previousButtons = m_currentButtons;
    m_mouseDelta = core::Vec2::zero();
    m_mouseScroll = core::Vec2::zero();
}

void InputState::onKeyDown(KeyCode key) noexcept {
    const auto idx = static_cast<core::usize>(key);
    if (idx < maxKeys) {
        m_currentKeys[idx] = true;
    }
}

void InputState::onKeyUp(KeyCode key) noexcept {
    const auto idx = static_cast<core::usize>(key);
    if (idx < maxKeys) {
        m_currentKeys[idx] = false;
    }
}

void InputState::onMouseButtonDown(MouseButton button) noexcept {
    const auto idx = static_cast<core::usize>(button);
    if (idx < maxButtons) {
        m_currentButtons[idx] = true;
    }
}

void InputState::onMouseButtonUp(MouseButton button) noexcept {
    const auto idx = static_cast<core::usize>(button);
    if (idx < maxButtons) {
        m_currentButtons[idx] = false;
    }
}

void InputState::onMouseMove(core::f32 x, core::f32 y, core::f32 dx, core::f32 dy) noexcept {
    m_mousePosition = core::Vec2(x, y);
    m_mouseDelta = core::Vec2(dx, dy);
}

void InputState::onMouseScroll(core::f32 sx, core::f32 sy) noexcept {
    m_mouseScroll = core::Vec2(sx, sy);
}

}  // namespace engine::platform
