#pragma once

#include <engine/core/types.hpp>
#include <engine/platform/input.hpp>

struct SDL_Window;

namespace engine::platform {

class Window;

class Platform {
public:
    static bool init();
    static void shutdown();
    static void pollEvents();

    [[nodiscard]] static const InputState& getInput() noexcept;
    [[nodiscard]] static InputState& getInputMutable() noexcept;
    [[nodiscard]] static bool isInitialized() noexcept;

    static void registerWindow(Window* window);
    static void unregisterWindow(Window* window);

private:
    static Window* findWindow(SDL_Window* nativeWindow);
};

}  // namespace engine::platform
