#include <utility>

#include <engine/core/log.hpp>
#include <engine/platform/platform.hpp>
#include <engine/platform/window.hpp>

#include <SDL3/SDL.h>

namespace engine::platform {

Window::Window(const WindowDesc& desc) : m_title(desc.title), m_width(desc.width), m_height(desc.height) {
    if (!Platform::isInitialized()) {
        Platform::init();
    }

    SDL_WindowFlags flags = SDL_WINDOW_VULKAN;
    if (desc.resizable) {
        flags |= SDL_WINDOW_RESIZABLE;
    }
    if (desc.highDpi) {
        flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
    }

    m_window = SDL_CreateWindow(desc.title.c_str(), static_cast<int>(desc.width), static_cast<int>(desc.height), flags);

    if (!m_window) {
        ENGINE_LOG_ERROR("Failed to create SDL3 window: {}", SDL_GetError());
        return;
    }

    int w = 0;
    int h = 0;
    SDL_GetWindowSizeInPixels(m_window, &w, &h);
    if (w > 0 && h > 0) {
        m_width = static_cast<core::u32>(w);
        m_height = static_cast<core::u32>(h);
    }

    Platform::registerWindow(this);
    ENGINE_LOG_INFO("Created window '{}' ({}x{})", m_title, m_width, m_height);
}

Window::~Window() {
    Platform::unregisterWindow(this);
    if (m_window) {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
        ENGINE_LOG_INFO("Destroyed window '{}'", m_title);
    }
}

Window::Window(Window&& other) noexcept
    : m_window(std::exchange(other.m_window, nullptr)), m_title(std::move(other.m_title)),
      m_width(std::exchange(other.m_width, 0)), m_height(std::exchange(other.m_height, 0)),
      m_shouldClose(std::exchange(other.m_shouldClose, false)) {
    Platform::registerWindow(this);
}

Window& Window::operator=(Window&& other) noexcept {
    if (this != &other) {
        Platform::unregisterWindow(this);
        if (m_window) {
            SDL_DestroyWindow(m_window);
        }
        m_window = std::exchange(other.m_window, nullptr);
        m_title = std::move(other.m_title);
        m_width = std::exchange(other.m_width, 0);
        m_height = std::exchange(other.m_height, 0);
        m_shouldClose = std::exchange(other.m_shouldClose, false);
        Platform::registerWindow(this);
    }
    return *this;
}

void Window::setTitle(const std::string& title) {
    m_title = title;
    if (m_window) {
        SDL_SetWindowTitle(m_window, m_title.c_str());
    }
}

void Window::onResize(core::u32 width, core::u32 height) noexcept {
    m_width = width;
    m_height = height;
    ENGINE_LOG_DEBUG("Window '{}' resized to {}x{}", m_title, m_width, m_height);
}

}  // namespace engine::platform
