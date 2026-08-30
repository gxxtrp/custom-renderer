#pragma once

#include <cassert>
#include <cmath>

#include <engine/core/math_utils.hpp>
#include <engine/core/types.hpp>
#include <engine/core/vec2.hpp>

namespace engine::core {

struct alignas(16) Vec3 {
    f32 x{0.0f};
    f32 y{0.0f};
    f32 z{0.0f};
    f32 _pad{0.0f};

    constexpr Vec3() noexcept = default;
    constexpr Vec3(f32 xVal, f32 yVal, f32 zVal) noexcept : x(xVal), y(yVal), z(zVal) {}
    constexpr Vec3(const Vec2& v, f32 zVal) noexcept : x(v.x), y(v.y), z(zVal) {}
    explicit constexpr Vec3(f32 scalar) noexcept : x(scalar), y(scalar), z(scalar) {}

    [[nodiscard]] constexpr f32 operator[](usize index) const noexcept {
        assert(index < 3);
        if (index == 0) {
            return x;
        }
        if (index == 1) {
            return y;
        }
        return z;
    }

    [[nodiscard]] constexpr f32& operator[](usize index) noexcept {
        assert(index < 3);
        if (index == 0) {
            return x;
        }
        if (index == 1) {
            return y;
        }
        return z;
    }

    [[nodiscard]] constexpr Vec2 xy() const noexcept { return {x, y}; }

    [[nodiscard]] constexpr Vec3 operator+(const Vec3& rhs) const noexcept { return {x + rhs.x, y + rhs.y, z + rhs.z}; }

    [[nodiscard]] constexpr Vec3 operator-(const Vec3& rhs) const noexcept { return {x - rhs.x, y - rhs.y, z - rhs.z}; }

    [[nodiscard]] constexpr Vec3 operator*(f32 scalar) const noexcept { return {x * scalar, y * scalar, z * scalar}; }

    [[nodiscard]] constexpr Vec3 operator*(const Vec3& rhs) const noexcept { return {x * rhs.x, y * rhs.y, z * rhs.z}; }

    [[nodiscard]] constexpr Vec3 operator/(f32 scalar) const noexcept {
        const f32 inv = 1.0f / scalar;
        return {x * inv, y * inv, z * inv};
    }

    [[nodiscard]] constexpr Vec3 operator/(const Vec3& rhs) const noexcept { return {x / rhs.x, y / rhs.y, z / rhs.z}; }

    [[nodiscard]] constexpr Vec3 operator-() const noexcept { return {-x, -y, -z}; }

    constexpr Vec3& operator+=(const Vec3& rhs) noexcept {
        x += rhs.x;
        y += rhs.y;
        z += rhs.z;
        return *this;
    }

    constexpr Vec3& operator-=(const Vec3& rhs) noexcept {
        x -= rhs.x;
        y -= rhs.y;
        z -= rhs.z;
        return *this;
    }

    constexpr Vec3& operator*=(f32 scalar) noexcept {
        x *= scalar;
        y *= scalar;
        z *= scalar;
        return *this;
    }

    constexpr Vec3& operator/=(f32 scalar) noexcept {
        const f32 inv = 1.0f / scalar;
        x *= inv;
        y *= inv;
        z *= inv;
        return *this;
    }

    [[nodiscard]] constexpr bool operator==(const Vec3& rhs) const noexcept {
        return approximatelyEqual(x, rhs.x) && approximatelyEqual(y, rhs.y) && approximatelyEqual(z, rhs.z);
    }

    [[nodiscard]] constexpr bool operator!=(const Vec3& rhs) const noexcept { return !(*this == rhs); }

    [[nodiscard]] constexpr f32 dot(const Vec3& rhs) const noexcept { return (x * rhs.x) + (y * rhs.y) + (z * rhs.z); }

    [[nodiscard]] constexpr Vec3 cross(const Vec3& rhs) const noexcept {
        return {(y * rhs.z) - (z * rhs.y), (z * rhs.x) - (x * rhs.z), (x * rhs.y) - (y * rhs.x)};
    }

    [[nodiscard]] constexpr f32 lengthSquared() const noexcept { return dot(*this); }

    [[nodiscard]] f32 length() const noexcept { return std::sqrt(lengthSquared()); }

    [[nodiscard]] Vec3 normalized() const noexcept {
        const f32 len = length();
        if (len > epsilon) {
            return *this / len;
        }
        return zero();
    }

    void normalize() noexcept { *this = normalized(); }

    [[nodiscard]] f32 distance(const Vec3& rhs) const noexcept { return (*this - rhs).length(); }

    [[nodiscard]] static constexpr Vec3 zero() noexcept { return {0.0f, 0.0f, 0.0f}; }
    [[nodiscard]] static constexpr Vec3 one() noexcept { return {1.0f, 1.0f, 1.0f}; }
    [[nodiscard]] static constexpr Vec3 unitX() noexcept { return {1.0f, 0.0f, 0.0f}; }
    [[nodiscard]] static constexpr Vec3 unitY() noexcept { return {0.0f, 1.0f, 0.0f}; }
    [[nodiscard]] static constexpr Vec3 unitZ() noexcept { return {0.0f, 0.0f, 1.0f}; }
    [[nodiscard]] static constexpr Vec3 up() noexcept { return {0.0f, 1.0f, 0.0f}; }
    [[nodiscard]] static constexpr Vec3 down() noexcept { return {0.0f, -1.0f, 0.0f}; }
    [[nodiscard]] static constexpr Vec3 forward() noexcept { return {0.0f, 0.0f, -1.0f}; }
    [[nodiscard]] static constexpr Vec3 backward() noexcept { return {0.0f, 0.0f, 1.0f}; }
    [[nodiscard]] static constexpr Vec3 right() noexcept { return {1.0f, 0.0f, 0.0f}; }
    [[nodiscard]] static constexpr Vec3 left() noexcept { return {-1.0f, 0.0f, 0.0f}; }
};

[[nodiscard]] constexpr Vec3 operator*(f32 scalar, const Vec3& v) noexcept {
    return v * scalar;
}

}  // namespace engine::core
