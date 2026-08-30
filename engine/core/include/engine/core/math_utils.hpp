#pragma once

#include <cmath>

#include <engine/core/types.hpp>

namespace engine::core {

inline constexpr f32 pi = 3.14159265358979323846f;
inline constexpr f32 twoPi = 2.0f * pi;
inline constexpr f32 halfPi = 0.5f * pi;
inline constexpr f32 deg2Rad = pi / 180.0f;
inline constexpr f32 rad2Deg = 180.0f / pi;
inline constexpr f32 epsilon = 1e-6f;

[[nodiscard]] constexpr f32 toRadians(f32 degrees) noexcept {
    return degrees * deg2Rad;
}

[[nodiscard]] constexpr f32 toDegrees(f32 radians) noexcept {
    return radians * rad2Deg;
}

[[nodiscard]] constexpr f32 clamp(f32 v, f32 minVal, f32 maxVal) noexcept {
    if (v < minVal) {
        return minVal;
    }
    if (v > maxVal) {
        return maxVal;
    }
    return v;
}

[[nodiscard]] constexpr f32 lerp(f32 a, f32 b, f32 t) noexcept {
    return a + (t * (b - a));
}

[[nodiscard]] inline bool approximatelyEqual(f32 a, f32 b, f32 eps = epsilon) noexcept {
    return std::abs(a - b) <= eps;
}

}  // namespace engine::core
