#include <utility>

#include <engine/core/log.hpp>
#include <engine/platform/platform.hpp>
#include <engine/platform/window.hpp>

#include <SDL3/SDL.h>

namespace engine::platform {

std::expected<Window, std::string> Window::create(const WindowDesc& desc) {
    if (!Platform::isInitialized()) {
        auto initResult = Platform::init();
        if (!initResult) {
            return std::unexpected(initResult.error());
        }
    }

    SDL_WindowFlags flags = SDL_WINDOW_VULKAN;
    if (desc.resizable) {
        flags |= SDL_WINDOW_RESIZABLE;
    }
    if (desc.highDpi) {
        flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
    }

    SDL_Window* nativeWindow =
        SDL_CreateWindow(desc.title.c_str(), static_cast<int>(desc.width), static_cast<int>(desc.height), flags);

    if (!nativeWindow) {
        std::string err = SDL_GetError();
        ENGINE_LOG_ERROR("Failed to create SDL3 window: {}", err);
        return std::unexpected(std::move(err));
    }

    int pixelWidth = 0;
    int pixelHeight = 0;
    SDL_GetWindowSizeInPixels(nativeWindow, &pixelWidth, &pixelHeight);

    const auto finalWidth = (pixelWidth > 0) ? static_cast<core::u32>(pixelWidth) : desc.width;
    const auto finalHeight = (pixelHeight > 0) ? static_cast<core::u32>(pixelHeight) : desc.height;

    ENGINE_LOG_INFO("Created window '{}' ({}x{})", desc.title, finalWidth, finalHeight);
    return Window(nativeWindow, desc.title, finalWidth, finalHeight);
}

Window::Window(SDL_Window* window, std::string title, core::u32 width, core::u32 height)
    : m_window(window), m_title(std::move(title)), m_width(width), m_height(height) {
    Platform::registerWindow(this);
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
