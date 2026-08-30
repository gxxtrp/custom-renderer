#pragma once

#include <cassert>
#include <cmath>

#include <engine/core/math_utils.hpp>
#include <engine/core/types.hpp>
#include <engine/core/vec2.hpp>
#include <engine/core/vec3.hpp>

namespace engine::core {

struct alignas(16) Vec4 {
    f32 x{0.0f};
    f32 y{0.0f};
    f32 z{0.0f};
    f32 w{0.0f};

    constexpr Vec4() noexcept = default;
    constexpr Vec4(f32 xVal, f32 yVal, f32 zVal, f32 wVal) noexcept : x(xVal), y(yVal), z(zVal), w(wVal) {}
    constexpr Vec4(const Vec3& v, f32 wVal) noexcept : x(v.x), y(v.y), z(v.z), w(wVal) {}
    constexpr Vec4(const Vec2& v, f32 zVal, f32 wVal) noexcept : x(v.x), y(v.y), z(zVal), w(wVal) {}
    explicit constexpr Vec4(f32 scalar) noexcept : x(scalar), y(scalar), z(scalar), w(scalar) {}

    [[nodiscard]] constexpr f32 operator[](usize index) const noexcept {
        assert(index < 4);
        if (index == 0) {
            return x;
        }
        if (index == 1) {
            return y;
        }
        if (index == 2) {
            return z;
        }
        return w;
    }

    [[nodiscard]] constexpr f32& operator[](usize index) noexcept {
        assert(index < 4);
        if (index == 0) {
            return x;
        }
        if (index == 1) {
            return y;
        }
        if (index == 2) {
            return z;
        }
        return w;
    }

    [[nodiscard]] constexpr Vec3 xyz() const noexcept { return {x, y, z}; }

    [[nodiscard]] constexpr Vec2 xy() const noexcept { return {x, y}; }

    [[nodiscard]] constexpr Vec4 operator+(const Vec4& rhs) const noexcept {
        return {x + rhs.x, y + rhs.y, z + rhs.z, w + rhs.w};
    }

    [[nodiscard]] constexpr Vec4 operator-(const Vec4& rhs) const noexcept {
        return {x - rhs.x, y - rhs.y, z - rhs.z, w - rhs.w};
    }

    [[nodiscard]] constexpr Vec4 operator*(f32 scalar) const noexcept {
        return {x * scalar, y * scalar, z * scalar, w * scalar};
    }

    [[nodiscard]] constexpr Vec4 operator*(const Vec4& rhs) const noexcept {
        return {x * rhs.x, y * rhs.y, z * rhs.z, w * rhs.w};
    }

    [[nodiscard]] constexpr Vec4 operator/(f32 scalar) const noexcept {
        const f32 inv = 1.0f / scalar;
        return {x * inv, y * inv, z * inv, w * inv};
    }

    [[nodiscard]] constexpr Vec4 operator/(const Vec4& rhs) const noexcept {
        return {x / rhs.x, y / rhs.y, z / rhs.z, w / rhs.w};
    }

    [[nodiscard]] constexpr Vec4 operator-() const noexcept { return {-x, -y, -z, -w}; }

    constexpr Vec4& operator+=(const Vec4& rhs) noexcept {
        x += rhs.x;
        y += rhs.y;
        z += rhs.z;
        w += rhs.w;
        return *this;
    }

    constexpr Vec4& operator-=(const Vec4& rhs) noexcept {
        x -= rhs.x;
        y -= rhs.y;
        z -= rhs.z;
        w -= rhs.w;
        return *this;
    }

    constexpr Vec4& operator*=(f32 scalar) noexcept {
        x *= scalar;
        y *= scalar;
        z *= scalar;
        w *= scalar;
        return *this;
    }

    constexpr Vec4& operator/=(f32 scalar) noexcept {
        const f32 inv = 1.0f / scalar;
        x *= inv;
        y *= inv;
        z *= inv;
        w *= inv;
        return *this;
    }

    [[nodiscard]] constexpr bool operator==(const Vec4& rhs) const noexcept {
        return approximatelyEqual(x, rhs.x) && approximatelyEqual(y, rhs.y) && approximatelyEqual(z, rhs.z) &&
               approximatelyEqual(w, rhs.w);
    }

    [[nodiscard]] constexpr bool operator!=(const Vec4& rhs) const noexcept { return !(*this == rhs); }

    [[nodiscard]] constexpr f32 dot(const Vec4& rhs) const noexcept {
        return (x * rhs.x) + (y * rhs.y) + (z * rhs.z) + (w * rhs.w);
    }

    [[nodiscard]] constexpr f32 lengthSquared() const noexcept { return dot(*this); }

    [[nodiscard]] f32 length() const noexcept { return std::sqrt(lengthSquared()); }

    [[nodiscard]] Vec4 normalized() const noexcept {
        const f32 len = length();
        if (len > epsilon) {
            return *this / len;
        }
        return zero();
    }

    void normalize() noexcept { *this = normalized(); }

    [[nodiscard]] static constexpr Vec4 zero() noexcept { return {0.0f, 0.0f, 0.0f, 0.0f}; }
    [[nodiscard]] static constexpr Vec4 one() noexcept { return {1.0f, 1.0f, 1.0f, 1.0f}; }
    [[nodiscard]] static constexpr Vec4 unitX() noexcept { return {1.0f, 0.0f, 0.0f, 0.0f}; }
    [[nodiscard]] static constexpr Vec4 unitY() noexcept { return {0.0f, 1.0f, 0.0f, 0.0f}; }
    [[nodiscard]] static constexpr Vec4 unitZ() noexcept { return {0.0f, 0.0f, 1.0f, 0.0f}; }
    [[nodiscard]] static constexpr Vec4 unitW() noexcept { return {0.0f, 0.0f, 0.0f, 1.0f}; }
};

[[nodiscard]] constexpr Vec4 operator*(f32 scalar, const Vec4& v) noexcept {
    return v * scalar;
}

}  // namespace engine::core
