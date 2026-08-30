#pragma once

#include <string>

#include <engine/core/types.hpp>

struct SDL_Window;

namespace engine::platform {

struct WindowDesc {
    std::string title{"Custom Engine"};
    core::u32 width{1280};
    core::u32 height{720};
    bool resizable{true};
    bool highDpi{true};
};

class Window {
public:
    explicit Window(const WindowDesc& desc);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    Window(Window&& other) noexcept;
    Window& operator=(Window&& other) noexcept;

    [[nodiscard]] core::u32 getWidth() const noexcept { return m_width; }
    [[nodiscard]] core::u32 getHeight() const noexcept { return m_height; }
    [[nodiscard]] core::f32 getAspectRatio() const noexcept {
        return (m_height > 0) ? (static_cast<core::f32>(m_width) / static_cast<core::f32>(m_height)) : 1.0f;
    }

    [[nodiscard]] bool shouldClose() const noexcept { return m_shouldClose; }
    void setShouldClose(bool close) noexcept { m_shouldClose = close; }

    [[nodiscard]] SDL_Window* getNativeHandle() const noexcept { return m_window; }
    [[nodiscard]] const std::string& getTitle() const noexcept { return m_title; }
    void setTitle(const std::string& title);

    void onResize(core::u32 width, core::u32 height) noexcept;

private:
    SDL_Window* m_window{nullptr};
    std::string m_title;
    core::u32 m_width{0};
    core::u32 m_height{0};
    bool m_shouldClose{false};
};

}  // namespace engine::platform
