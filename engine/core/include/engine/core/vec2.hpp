#pragma once

#include <cassert>
#include <cmath>

#include <engine/core/math_utils.hpp>
#include <engine/core/types.hpp>

namespace engine::core {

struct alignas(8) Vec2 {
    f32 x{0.0f};
    f32 y{0.0f};

    constexpr Vec2() noexcept = default;
    constexpr Vec2(f32 xVal, f32 yVal) noexcept : x(xVal), y(yVal) {}
    explicit constexpr Vec2(f32 scalar) noexcept : x(scalar), y(scalar) {}

    [[nodiscard]] constexpr f32 operator[](usize index) const noexcept {
        assert(index < 2);
        return (index == 0) ? x : y;
    }

    [[nodiscard]] constexpr f32& operator[](usize index) noexcept {
        assert(index < 2);
        return (index == 0) ? x : y;
    }

    [[nodiscard]] constexpr Vec2 operator+(const Vec2& rhs) const noexcept { return {x + rhs.x, y + rhs.y}; }

    [[nodiscard]] constexpr Vec2 operator-(const Vec2& rhs) const noexcept { return {x - rhs.x, y - rhs.y}; }

    [[nodiscard]] constexpr Vec2 operator*(f32 scalar) const noexcept { return {x * scalar, y * scalar}; }

    [[nodiscard]] constexpr Vec2 operator*(const Vec2& rhs) const noexcept { return {x * rhs.x, y * rhs.y}; }

    [[nodiscard]] constexpr Vec2 operator/(f32 scalar) const noexcept {
        const f32 inv = 1.0f / scalar;
        return {x * inv, y * inv};
    }

    [[nodiscard]] constexpr Vec2 operator/(const Vec2& rhs) const noexcept { return {x / rhs.x, y / rhs.y}; }

    [[nodiscard]] constexpr Vec2 operator-() const noexcept { return {-x, -y}; }

    constexpr Vec2& operator+=(const Vec2& rhs) noexcept {
        x += rhs.x;
        y += rhs.y;
        return *this;
    }

    constexpr Vec2& operator-=(const Vec2& rhs) noexcept {
        x -= rhs.x;
        y -= rhs.y;
        return *this;
    }

    constexpr Vec2& operator*=(f32 scalar) noexcept {
        x *= scalar;
        y *= scalar;
        return *this;
    }

    constexpr Vec2& operator/=(f32 scalar) noexcept {
        const f32 inv = 1.0f / scalar;
        x *= inv;
        y *= inv;
        return *this;
    }

    [[nodiscard]] constexpr bool operator==(const Vec2& rhs) const noexcept {
        return approximatelyEqual(x, rhs.x) && approximatelyEqual(y, rhs.y);
    }

    [[nodiscard]] constexpr bool operator!=(const Vec2& rhs) const noexcept { return !(*this == rhs); }

    [[nodiscard]] constexpr f32 dot(const Vec2& rhs) const noexcept { return (x * rhs.x) + (y * rhs.y); }

    [[nodiscard]] constexpr f32 lengthSquared() const noexcept { return dot(*this); }

    [[nodiscard]] f32 length() const noexcept { return std::sqrt(lengthSquared()); }

    [[nodiscard]] Vec2 normalized() const noexcept {
        const f32 len = length();
        if (len > epsilon) {
            return *this / len;
        }
        return zero();
    }

    void normalize() noexcept { *this = normalized(); }

    [[nodiscard]] f32 distance(const Vec2& rhs) const noexcept { return (*this - rhs).length(); }

    [[nodiscard]] static constexpr Vec2 zero() noexcept { return {0.0f, 0.0f}; }
    [[nodiscard]] static constexpr Vec2 one() noexcept { return {1.0f, 1.0f}; }
    [[nodiscard]] static constexpr Vec2 unitX() noexcept { return {1.0f, 0.0f}; }
    [[nodiscard]] static constexpr Vec2 unitY() noexcept { return {0.0f, 1.0f}; }
};

[[nodiscard]] constexpr Vec2 operator*(f32 scalar, const Vec2& v) noexcept {
    return v * scalar;
}

}  // namespace engine::core
