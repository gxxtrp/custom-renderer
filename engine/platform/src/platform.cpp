#include <algorithm>
#include <vector>

#include <engine/core/log.hpp>
#include <engine/core/profiler.hpp>
#include <engine/platform/platform.hpp>
#include <engine/platform/window.hpp>

#include <SDL3/SDL.h>

namespace engine::platform {

namespace {

bool sInitialized = false;
InputState sInputState;
std::vector<Window*> sWindows;

KeyCode translateScancode(SDL_Scancode scancode) {
    if (scancode >= SDL_SCANCODE_A && scancode <= SDL_SCANCODE_Z) {
        return static_cast<KeyCode>(static_cast<core::u16>(KeyCode::A) +
                                    static_cast<core::u16>(scancode - SDL_SCANCODE_A));
    }
    if (scancode >= SDL_SCANCODE_1 && scancode <= SDL_SCANCODE_9) {
        return static_cast<KeyCode>(static_cast<core::u16>(KeyCode::Num1) +
                                    static_cast<core::u16>(scancode - SDL_SCANCODE_1));
    }
    if (scancode == SDL_SCANCODE_0) {
        return KeyCode::Num0;
    }
    if (scancode >= SDL_SCANCODE_F1 && scancode <= SDL_SCANCODE_F12) {
        return static_cast<KeyCode>(static_cast<core::u16>(KeyCode::F1) +
                                    static_cast<core::u16>(scancode - SDL_SCANCODE_F1));
    }

    switch (scancode) {
    case SDL_SCANCODE_ESCAPE:
        return KeyCode::Escape;
    case SDL_SCANCODE_RETURN:
        return KeyCode::Enter;
    case SDL_SCANCODE_TAB:
        return KeyCode::Tab;
    case SDL_SCANCODE_BACKSPACE:
        return KeyCode::Backspace;
    case SDL_SCANCODE_INSERT:
        return KeyCode::Insert;
    case SDL_SCANCODE_DELETE:
        return KeyCode::Delete;
    case SDL_SCANCODE_RIGHT:
        return KeyCode::Right;
    case SDL_SCANCODE_LEFT:
        return KeyCode::Left;
    case SDL_SCANCODE_DOWN:
        return KeyCode::Down;
    case SDL_SCANCODE_UP:
        return KeyCode::Up;
    case SDL_SCANCODE_PAGEUP:
        return KeyCode::PageUp;
    case SDL_SCANCODE_PAGEDOWN:
        return KeyCode::PageDown;
    case SDL_SCANCODE_HOME:
        return KeyCode::Home;
    case SDL_SCANCODE_END:
        return KeyCode::End;
    case SDL_SCANCODE_CAPSLOCK:
        return KeyCode::CapsLock;
    case SDL_SCANCODE_SCROLLLOCK:
        return KeyCode::ScrollLock;
    case SDL_SCANCODE_NUMLOCKCLEAR:
        return KeyCode::NumLock;
    case SDL_SCANCODE_PRINTSCREEN:
        return KeyCode::PrintScreen;
    case SDL_SCANCODE_PAUSE:
        return KeyCode::Pause;
    case SDL_SCANCODE_LSHIFT:
        return KeyCode::LeftShift;
    case SDL_SCANCODE_LCTRL:
        return KeyCode::LeftControl;
    case SDL_SCANCODE_LALT:
        return KeyCode::LeftAlt;
    case SDL_SCANCODE_LGUI:
        return KeyCode::LeftSuper;
    case SDL_SCANCODE_RSHIFT:
        return KeyCode::RightShift;
    case SDL_SCANCODE_RCTRL:
        return KeyCode::RightControl;
    case SDL_SCANCODE_RALT:
        return KeyCode::RightAlt;
    case SDL_SCANCODE_RGUI:
        return KeyCode::RightSuper;
    case SDL_SCANCODE_SPACE:
        return KeyCode::Space;
    default:
        return KeyCode::Unknown;
    }
}

MouseButton translateMouseButton(core::u8 button) {
    switch (button) {
    case SDL_BUTTON_LEFT:
        return MouseButton::Left;
    case SDL_BUTTON_MIDDLE:
        return MouseButton::Middle;
    case SDL_BUTTON_RIGHT:
        return MouseButton::Right;
    case SDL_BUTTON_X1:
        return MouseButton::X1;
    case SDL_BUTTON_X2:
        return MouseButton::X2;
    default:
        return MouseButton::Left;
    }
}

}  // namespace

std::expected<void, std::string> Platform::init() {
    if (sInitialized) {
        return {};
    }

    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS)) {
        std::string err = SDL_GetError();
        ENGINE_LOG_CRITICAL("Failed to initialize SDL3: {}", err);
        return std::unexpected(std::move(err));
    }

    sInitialized = true;
    ENGINE_LOG_INFO("Initialized SDL3 platform subsystem");
    return {};
}

void Platform::shutdown() {
    if (!sInitialized) {
        return;
    }

    sWindows.clear();
    SDL_Quit();
    sInitialized = false;
    ENGINE_LOG_INFO("Shutdown SDL3 platform subsystem");
}

void Platform::pollEvents() {
    ENGINE_PROFILE_ZONE_NAMED("Platform::pollEvents");

    sInputState.newFrame();

    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_EVENT_QUIT: {
            for (auto* win : sWindows) {
                if (win) {
                    win->setShouldClose(true);
                }
            }
            break;
        }

        case SDL_EVENT_WINDOW_CLOSE_REQUESTED: {
            SDL_Window* nativeWin = SDL_GetWindowFromID(event.window.windowID);
            Window* win = findWindow(nativeWin);
            if (win) {
                win->setShouldClose(true);
            }
            break;
        }

        case SDL_EVENT_WINDOW_PIXEL_SIZE_CHANGED:
        case SDL_EVENT_WINDOW_RESIZED: {
            SDL_Window* nativeWin = SDL_GetWindowFromID(event.window.windowID);
            Window* win = findWindow(nativeWin);
            if (win) {
                win->onResize(static_cast<core::u32>(event.window.data1), static_cast<core::u32>(event.window.data2));
            }
            break;
        }

        case SDL_EVENT_KEY_DOWN: {
            const KeyCode code = translateScancode(event.key.scancode);
            if (code != KeyCode::Unknown) {
                sInputState.onKeyDown(code);
            }
            break;
        }

        case SDL_EVENT_KEY_UP: {
            const KeyCode code = translateScancode(event.key.scancode);
            if (code != KeyCode::Unknown) {
                sInputState.onKeyUp(code);
            }
            break;
        }

        case SDL_EVENT_MOUSE_MOTION: {
            sInputState.onMouseMove(event.motion.x, event.motion.y, event.motion.xrel, event.motion.yrel);
            break;
        }

        case SDL_EVENT_MOUSE_BUTTON_DOWN: {
            sInputState.onMouseButtonDown(translateMouseButton(event.button.button));
            break;
        }

        case SDL_EVENT_MOUSE_BUTTON_UP: {
            sInputState.onMouseButtonUp(translateMouseButton(event.button.button));
            break;
        }

        case SDL_EVENT_MOUSE_WHEEL: {
            sInputState.onMouseScroll(event.wheel.x, event.wheel.y);
            break;
        }

        default:
            break;
        }
    }
}

const InputState& Platform::getInput() noexcept {
    return sInputState;
}

InputState& Platform::getInputMutable() noexcept {
    return sInputState;
}

bool Platform::isInitialized() noexcept {
    return sInitialized;
}

std::span<Window* const> Platform::getWindows() noexcept {
    return sWindows;
}

void Platform::registerWindow(Window* window) {
    if (window && std::ranges::find(sWindows, window) == sWindows.end()) {
        sWindows.push_back(window);
    }
}

void Platform::unregisterWindow(Window* window) {
    auto it = std::ranges::find(sWindows, window);
    if (it != sWindows.end()) {
        sWindows.erase(it);
    }
}

Window* Platform::findWindow(SDL_Window* nativeWindow) {
    for (auto* win : sWindows) {
        if (win && win->getNativeHandle() == nativeWindow) {
            return win;
        }
    }
    return nullptr;
}

}  // namespace engine::platform
