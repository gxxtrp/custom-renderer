#pragma once

#include <cmath>

#include <engine/core/mat4.hpp>
#include <engine/core/math_utils.hpp>
#include <engine/core/types.hpp>
#include <engine/core/vec3.hpp>

namespace engine::core {

struct alignas(16) Quat {
    f32 x{0.0f};
    f32 y{0.0f};
    f32 z{0.0f};
    f32 w{1.0f};

    constexpr Quat() noexcept = default;
    constexpr Quat(f32 xVal, f32 yVal, f32 zVal, f32 wVal) noexcept : x(xVal), y(yVal), z(zVal), w(wVal) {}

    [[nodiscard]] static constexpr Quat identity() noexcept { return {0.0f, 0.0f, 0.0f, 1.0f}; }

    [[nodiscard]] static Quat fromAxisAngle(const Vec3& axis, f32 angleRad) noexcept {
        const f32 halfAngle = angleRad * 0.5f;
        const f32 s = std::sin(halfAngle);
        const Vec3 normalizedAxis = axis.normalized();
        return {normalizedAxis.x * s, normalizedAxis.y * s, normalizedAxis.z * s, std::cos(halfAngle)};
    }

    [[nodiscard]] static Quat fromEuler(f32 pitchRad, f32 yawRad, f32 rollRad) noexcept {
        const f32 cy = std::cos(yawRad * 0.5f);
        const f32 sy = std::sin(yawRad * 0.5f);
        const f32 cp = std::cos(pitchRad * 0.5f);
        const f32 sp = std::sin(pitchRad * 0.5f);
        const f32 cr = std::cos(rollRad * 0.5f);
        const f32 sr = std::sin(rollRad * 0.5f);

        return {(sr * cp * cy) - (cr * sp * sy), (cr * sp * cy) + (sr * cp * sy), (cr * cp * sy) - (sr * sp * cy),
                (cr * cp * cy) + (sr * sp * sy)};
    }

    [[nodiscard]] constexpr f32 dot(const Quat& rhs) const noexcept {
        return (x * rhs.x) + (y * rhs.y) + (z * rhs.z) + (w * rhs.w);
    }

    [[nodiscard]] constexpr f32 lengthSquared() const noexcept { return dot(*this); }

    [[nodiscard]] f32 length() const noexcept { return std::sqrt(lengthSquared()); }

    [[nodiscard]] Quat normalized() const noexcept {
        const f32 len = length();
        if (len > epsilon) {
            const f32 inv = 1.0f / len;
            return {x * inv, y * inv, z * inv, w * inv};
        }
        return identity();
    }

    void normalize() noexcept { *this = normalized(); }

    [[nodiscard]] constexpr Quat conjugated() const noexcept { return {-x, -y, -z, w}; }

    [[nodiscard]] Quat inversed() const noexcept {
        const f32 lenSq = lengthSquared();
        if (lenSq > epsilon) {
            const f32 inv = 1.0f / lenSq;
            return {-x * inv, -y * inv, -z * inv, w * inv};
        }
        return identity();
    }

    [[nodiscard]] constexpr Quat operator*(const Quat& rhs) const noexcept {
        return {(w * rhs.x) + (x * rhs.w) + (y * rhs.z) - (z * rhs.y),
                (w * rhs.y) - (x * rhs.z) + (y * rhs.w) + (z * rhs.x),
                (w * rhs.z) + (x * rhs.y) - (y * rhs.x) + (z * rhs.w),
                (w * rhs.w) - (x * rhs.x) - (y * rhs.y) - (z * rhs.z)};
    }

    [[nodiscard]] constexpr Vec3 operator*(const Vec3& v) const noexcept {
        const Vec3 qv(x, y, z);
        const Vec3 uv = qv.cross(v);
        const Vec3 uuv = qv.cross(uv);
        return v + (((uv * w) + uuv) * 2.0f);
    }

    [[nodiscard]] constexpr Mat4 toMat4() const noexcept {
        const f32 xx = x * x;
        const f32 yy = y * y;
        const f32 zz = z * z;
        const f32 xy = x * y;
        const f32 xz = x * z;
        const f32 yz = y * z;
        const f32 wx = w * x;
        const f32 wy = w * y;
        const f32 wz = w * z;

        Mat4 m = Mat4::identity();
        m.columns[0] = Vec4(1.0f - (2.0f * (yy + zz)), 2.0f * (xy + wz), 2.0f * (xz - wy), 0.0f);
        m.columns[1] = Vec4(2.0f * (xy - wz), 1.0f - (2.0f * (xx + zz)), 2.0f * (yz + wx), 0.0f);
        m.columns[2] = Vec4(2.0f * (xz + wy), 2.0f * (yz - wx), 1.0f - (2.0f * (xx + yy)), 0.0f);
        m.columns[3] = Vec4(0.0f, 0.0f, 0.0f, 1.0f);
        return m;
    }

    [[nodiscard]] static Quat slerp(const Quat& a, Quat b, f32 t) noexcept {
        f32 cosTheta = a.dot(b);

        if (cosTheta < 0.0f) {
            b = Quat(-b.x, -b.y, -b.z, -b.w);
            cosTheta = -cosTheta;
        }

        if (cosTheta > 1.0f - epsilon) {
            return Quat(lerp(a.x, b.x, t), lerp(a.y, b.y, t), lerp(a.z, b.z, t), lerp(a.w, b.w, t)).normalized();
        }

        const f32 theta = std::acos(cosTheta);
        const f32 sinTheta = std::sin(theta);
        const f32 wA = std::sin((1.0f - t) * theta) / sinTheta;
        const f32 wB = std::sin(t * theta) / sinTheta;

        return {(wA * a.x) + (wB * b.x), (wA * a.y) + (wB * b.y), (wA * a.z) + (wB * b.z), (wA * a.w) + (wB * b.w)};
    }
};

}  // namespace engine::core
