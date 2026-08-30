#pragma once

#include <chrono>

#include <engine/core/types.hpp>

namespace tests::common {

class Timer {
public:
    Timer() noexcept : m_startTime(std::chrono::high_resolution_clock::now()), m_lastTime(m_startTime) {}

    void reset() noexcept {
        m_startTime = std::chrono::high_resolution_clock::now();
        m_lastTime = m_startTime;
    }

    [[nodiscard]] engine::core::f32 tick() noexcept {
        const auto now = std::chrono::high_resolution_clock::now();
        const std::chrono::duration<engine::core::f32> delta = now - m_lastTime;
        m_lastTime = now;
        return delta.count();
    }

    [[nodiscard]] engine::core::f32 elapsedSeconds() const noexcept {
        const auto now = std::chrono::high_resolution_clock::now();
        const std::chrono::duration<engine::core::f32> elapsed = now - m_startTime;
        return elapsed.count();
    }

private:
    std::chrono::high_resolution_clock::time_point m_startTime;
    std::chrono::high_resolution_clock::time_point m_lastTime;
};

}  // namespace tests::common
