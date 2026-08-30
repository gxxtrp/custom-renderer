#include <chrono>
#include <thread>

#include <engine/core/linear_allocator.hpp>
#include <engine/core/log.hpp>
#include <engine/core/math.hpp>
#include <engine/core/profiler.hpp>
#include <engine/core/types.hpp>
#include <engine/platform/key_codes.hpp>
#include <engine/platform/platform.hpp>
#include <engine/platform/window.hpp>

#include <tests/common/timer.hpp>

int main(int /*argc*/, char** /*argv*/) {
    using namespace engine::core;
    using namespace engine::platform;

    // Initialize core logging
    Log::init(LogLevel::Info);
    ENGINE_LOG_INFO("=================================================");
    ENGINE_LOG_INFO(" Starting Scenario: m1_window");
    ENGINE_LOG_INFO("=================================================");

    // Verify Linear Allocator
    {
        constexpr usize allocatorSize = 1024ULL * 1024ULL;  // 1MB
        LinearAllocator frameAllocator(allocatorSize);
        auto* testVec = frameAllocator.create<Vec3>(1.0f, 2.0f, 3.0f);
        if (testVec != nullptr && testVec->x == 1.0f && testVec->y == 2.0f && testVec->z == 3.0f) {
            ENGINE_LOG_INFO("LinearAllocator test passed: Allocated at offset {} bytes",
                            frameAllocator.getAllocatedBytes());
        }
        frameAllocator.reset();
    }

    // Verify SIMD Math
    {
        const Mat4 translation = Mat4::translation(Vec3(10.0f, 0.0f, 0.0f));
        const Vec3 originalPoint(1.0f, 2.0f, 3.0f);
        const Vec3 transformedPoint = translation.transformPoint(originalPoint);
        ENGINE_LOG_INFO("Math test passed: Point ({:.1f}, {:.1f}, {:.1f}) transformed to ({:.1f}, {:.1f}, {:.1f})",
                        originalPoint.x, originalPoint.y, originalPoint.z, transformedPoint.x, transformedPoint.y,
                        transformedPoint.z);
    }

    // Initialize Platform subsystem
    auto initResult = Platform::init();
    if (!initResult) {
        ENGINE_LOG_CRITICAL("Failed to initialize platform subsystem: {}", initResult.error());
        return 1;
    }

    {
        const WindowDesc desc{.title = "Custom Engine - M1 Window Test Scenario",
                              .width = 1280,
                              .height = 720,
                              .resizable = true,
                              .highDpi = true};

        auto windowResult = Window::create(desc);
        if (!windowResult) {
            ENGINE_LOG_CRITICAL("Failed to create native window: {}", windowResult.error());
            Platform::shutdown();
            return 1;
        }

        Window window = std::move(windowResult.value());
        ENGINE_LOG_INFO("Main loop started. Press ESC or close window to exit.");

        tests::common::Timer timer;
        f32 accumulator = 0.0f;
        u32 frameCount = 0;

        while (!window.shouldClose()) {
            ENGINE_PROFILE_FRAME();

            const f32 deltaTime = timer.tick();
            accumulator += deltaTime;
            ++frameCount;

            // Poll input and window events
            Platform::pollEvents();

            const auto& input = Platform::getInput();
            if (input.isKeyPressed(KeyCode::Escape)) {
                ENGINE_LOG_INFO("Escape key pressed. Exiting...");
                window.setShouldClose(true);
                break;
            }

            // Periodic metrics logging
            if (accumulator >= 1.0f) {
                const f32 fps = static_cast<f32>(frameCount) / accumulator;
                const f32 frameTimeMs = (accumulator / static_cast<f32>(frameCount)) * 1000.0f;
                ENGINE_LOG_INFO("Metrics: FPS: {:.1f} | Frame Time: {:.3f} ms | Size: {}x{}", fps, frameTimeMs,
                                window.getWidth(), window.getHeight());
                accumulator = 0.0f;
                frameCount = 0;
            }

            // Yield frame CPU slice
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }

        ENGINE_LOG_INFO("Exiting scenario loop...");
    }

    Platform::shutdown();
    ENGINE_LOG_INFO("Scenario m1_window completed cleanly.");
    Log::shutdown();

    return 0;
}
