#pragma once

#include <cassert>
#include <cmath>

#include <engine/core/math_utils.hpp>
#include <engine/core/types.hpp>
#include <engine/core/vec3.hpp>
#include <engine/core/vec4.hpp>

namespace engine::core {

struct alignas(16) Mat4 {
    Vec4 columns[4];  // Column-major: columns[col][row]

    constexpr Mat4() noexcept {
        columns[0] = {1.0f, 0.0f, 0.0f, 0.0f};
        columns[1] = {0.0f, 1.0f, 0.0f, 0.0f};
        columns[2] = {0.0f, 0.0f, 1.0f, 0.0f};
        columns[3] = {0.0f, 0.0f, 0.0f, 1.0f};
    }

    constexpr Mat4(const Vec4& c0, const Vec4& c1, const Vec4& c2, const Vec4& c3) noexcept {
        columns[0] = c0;
        columns[1] = c1;
        columns[2] = c2;
        columns[3] = c3;
    }

    [[nodiscard]] constexpr const Vec4& operator[](usize col) const noexcept {
        assert(col < 4);
        return columns[col];
    }

    [[nodiscard]] constexpr Vec4& operator[](usize col) noexcept {
        assert(col < 4);
        return columns[col];
    }

    [[nodiscard]] constexpr f32 operator()(usize row, usize col) const noexcept {
        assert(row < 4 && col < 4);
        return columns[col][row];
    }

    [[nodiscard]] constexpr f32& operator()(usize row, usize col) noexcept {
        assert(row < 4 && col < 4);
        return columns[col][row];
    }

    [[nodiscard]] const f32* data() const noexcept { return &columns[0].x; }

    [[nodiscard]] f32* data() noexcept { return &columns[0].x; }

    [[nodiscard]] static constexpr Mat4 identity() noexcept { return Mat4(); }

    [[nodiscard]] static constexpr Mat4 zeros() noexcept {
        return Mat4(Vec4::zero(), Vec4::zero(), Vec4::zero(), Vec4::zero());
    }

    [[nodiscard]] static constexpr Mat4 translation(const Vec3& t) noexcept {
        Mat4 m;
        m.columns[3] = Vec4(t, 1.0f);
        return m;
    }

    [[nodiscard]] static constexpr Mat4 scaling(const Vec3& s) noexcept {
        Mat4 m = zeros();
        m.columns[0].x = s.x;
        m.columns[1].y = s.y;
        m.columns[2].z = s.z;
        m.columns[3].w = 1.0f;
        return m;
    }

    [[nodiscard]] static constexpr Mat4 scaling(f32 s) noexcept { return scaling(Vec3(s)); }

    [[nodiscard]] static Mat4 rotationX(f32 angleRad) noexcept {
        const f32 c = std::cos(angleRad);
        const f32 s = std::sin(angleRad);

        Mat4 m = identity();
        m.columns[1].y = c;
        m.columns[1].z = s;
        m.columns[2].y = -s;
        m.columns[2].z = c;
        return m;
    }

    [[nodiscard]] static Mat4 rotationY(f32 angleRad) noexcept {
        const f32 c = std::cos(angleRad);
        const f32 s = std::sin(angleRad);

        Mat4 m = identity();
        m.columns[0].x = c;
        m.columns[0].z = -s;
        m.columns[2].x = s;
        m.columns[2].z = c;
        return m;
    }

    [[nodiscard]] static Mat4 rotationZ(f32 angleRad) noexcept {
        const f32 c = std::cos(angleRad);
        const f32 s = std::sin(angleRad);

        Mat4 m = identity();
        m.columns[0].x = c;
        m.columns[0].y = s;
        m.columns[1].x = -s;
        m.columns[1].y = c;
        return m;
    }

    [[nodiscard]] static Mat4 rotationAxis(const Vec3& axis, f32 angleRad) noexcept {
        const Vec3 a = axis.normalized();
        const f32 c = std::cos(angleRad);
        const f32 s = std::sin(angleRad);
        const f32 omc = 1.0f - c;

        Mat4 m = zeros();
        m.columns[0] = Vec4(c + (a.x * a.x * omc), (a.y * a.x * omc) + (a.z * s), (a.z * a.x * omc) - (a.y * s), 0.0f);
        m.columns[1] = Vec4((a.x * a.y * omc) - (a.z * s), c + (a.y * a.y * omc), (a.z * a.y * omc) + (a.x * s), 0.0f);
        m.columns[2] = Vec4((a.x * a.z * omc) + (a.y * s), (a.y * a.z * omc) - (a.x * s), c + (a.z * a.z * omc), 0.0f);
        m.columns[3] = Vec4(0.0f, 0.0f, 0.0f, 1.0f);
        return m;
    }

    [[nodiscard]] static Mat4 perspective(f32 fovYRad, f32 aspect, f32 zNear, f32 zFar) noexcept {
        assert(aspect > 0.0f && zFar > zNear && fovYRad > 0.0f);
        const f32 tanHalfFov = std::tan(fovYRad * 0.5f);

        Mat4 m = zeros();
        m.columns[0].x = 1.0f / (aspect * tanHalfFov);
        m.columns[1].y = 1.0f / tanHalfFov;
        m.columns[2].z = zFar / (zNear - zFar);
        m.columns[2].w = -1.0f;
        m.columns[3].z = -(zFar * zNear) / (zFar - zNear);
        return m;
    }

    [[nodiscard]] static Mat4 orthographic(f32 left, f32 right, f32 bottom, f32 top, f32 zNear, f32 zFar) noexcept {
        Mat4 m = zeros();
        m.columns[0].x = 2.0f / (right - left);
        m.columns[1].y = 2.0f / (top - bottom);
        m.columns[2].z = 1.0f / (zNear - zFar);
        m.columns[3].x = -(right + left) / (right - left);
        m.columns[3].y = -(top + bottom) / (top - bottom);
        m.columns[3].z = zNear / (zNear - zFar);
        m.columns[3].w = 1.0f;
        return m;
    }

    [[nodiscard]] static Mat4 lookAt(const Vec3& eye, const Vec3& target, const Vec3& up) noexcept {
        const Vec3 f = (target - eye).normalized();
        const Vec3 r = f.cross(up).normalized();
        const Vec3 u = r.cross(f);

        Mat4 m = identity();
        m.columns[0].x = r.x;
        m.columns[1].x = r.y;
        m.columns[2].x = r.z;

        m.columns[0].y = u.x;
        m.columns[1].y = u.y;
        m.columns[2].y = u.z;

        m.columns[0].z = -f.x;
        m.columns[1].z = -f.y;
        m.columns[2].z = -f.z;

        m.columns[3].x = -r.dot(eye);
        m.columns[3].y = -u.dot(eye);
        m.columns[3].z = f.dot(eye);
        return m;
    }

    [[nodiscard]] constexpr Mat4 operator*(const Mat4& rhs) const noexcept {
        Mat4 res = zeros();
        for (usize c = 0; c < 4; ++c) {
            for (usize r = 0; r < 4; ++r) {
                res.columns[c][r] = (columns[0][r] * rhs.columns[c][0]) + (columns[1][r] * rhs.columns[c][1]) +
                                    (columns[2][r] * rhs.columns[c][2]) + (columns[3][r] * rhs.columns[c][3]);
            }
        }
        return res;
    }

    [[nodiscard]] constexpr Vec4 operator*(const Vec4& v) const noexcept {
        return {(columns[0].x * v.x) + (columns[1].x * v.y) + (columns[2].x * v.z) + (columns[3].x * v.w),
                (columns[0].y * v.x) + (columns[1].y * v.y) + (columns[2].y * v.z) + (columns[3].y * v.w),
                (columns[0].z * v.x) + (columns[1].z * v.y) + (columns[2].z * v.z) + (columns[3].z * v.w),
                (columns[0].w * v.x) + (columns[1].w * v.y) + (columns[2].w * v.z) + (columns[3].w * v.w)};
    }

    [[nodiscard]] constexpr Vec3 transformPoint(const Vec3& p) const noexcept {
        const Vec4 res = *this * Vec4(p, 1.0f);
        if (std::abs(res.w) > epsilon) {
            return res.xyz() / res.w;
        }
        return res.xyz();
    }

    [[nodiscard]] constexpr Vec3 transformVector(const Vec3& v) const noexcept {
        const Vec4 res = *this * Vec4(v, 0.0f);
        return res.xyz();
    }

    [[nodiscard]] constexpr Mat4 transposed() const noexcept {
        return Mat4(Vec4(columns[0].x, columns[1].x, columns[2].x, columns[3].x),
                    Vec4(columns[0].y, columns[1].y, columns[2].y, columns[3].y),
                    Vec4(columns[0].z, columns[1].z, columns[2].z, columns[3].z),
                    Vec4(columns[0].w, columns[1].w, columns[2].w, columns[3].w));
    }

    [[nodiscard]] Mat4 inversed() const noexcept {
        const f32* m = data();

        f32 inv[16];

        inv[0] = (m[5] * m[10] * m[15]) - (m[5] * m[11] * m[14]) - (m[9] * m[6] * m[15]) + (m[9] * m[7] * m[14]) +
                 (m[13] * m[6] * m[11]) - (m[13] * m[7] * m[10]);

        inv[4] = -(m[4] * m[10] * m[15]) + (m[4] * m[11] * m[14]) + (m[8] * m[6] * m[15]) - (m[8] * m[7] * m[14]) -
                 (m[12] * m[6] * m[11]) + (m[12] * m[7] * m[10]);

        inv[8] = (m[4] * m[9] * m[15]) - (m[4] * m[11] * m[13]) - (m[8] * m[5] * m[15]) + (m[8] * m[7] * m[13]) +
                 (m[12] * m[5] * m[11]) - (m[12] * m[7] * m[9]);

        inv[12] = -(m[4] * m[9] * m[14]) + (m[4] * m[10] * m[13]) + (m[8] * m[5] * m[14]) - (m[8] * m[6] * m[13]) -
                  (m[12] * m[5] * m[10]) + (m[12] * m[6] * m[9]);

        inv[1] = -(m[1] * m[10] * m[15]) + (m[1] * m[11] * m[14]) + (m[9] * m[2] * m[15]) - (m[9] * m[3] * m[14]) -
                 (m[13] * m[2] * m[11]) + (m[13] * m[3] * m[10]);

        inv[5] = (m[0] * m[10] * m[15]) - (m[0] * m[11] * m[14]) - (m[8] * m[2] * m[15]) + (m[8] * m[3] * m[14]) +
                 (m[12] * m[2] * m[11]) - (m[12] * m[3] * m[10]);

        inv[9] = -(m[0] * m[9] * m[15]) + (m[0] * m[11] * m[13]) + (m[8] * m[1] * m[15]) - (m[8] * m[3] * m[13]) -
                 (m[12] * m[1] * m[11]) + (m[12] * m[3] * m[9]);

        inv[13] = (m[0] * m[9] * m[14]) - (m[0] * m[10] * m[13]) - (m[8] * m[1] * m[14]) + (m[8] * m[2] * m[13]) +
                  (m[12] * m[1] * m[10]) - (m[12] * m[2] * m[9]);

        inv[2] = (m[1] * m[6] * m[15]) - (m[1] * m[7] * m[14]) - (m[5] * m[2] * m[15]) + (m[5] * m[3] * m[14]) +
                 (m[13] * m[2] * m[7]) - (m[13] * m[3] * m[6]);

        inv[6] = -(m[0] * m[6] * m[15]) + (m[0] * m[7] * m[14]) + (m[4] * m[2] * m[15]) - (m[4] * m[3] * m[14]) -
                 (m[12] * m[2] * m[7]) + (m[12] * m[3] * m[6]);

        inv[10] = (m[0] * m[5] * m[15]) - (m[0] * m[7] * m[13]) - (m[4] * m[1] * m[15]) + (m[4] * m[3] * m[13]) +
                  (m[12] * m[1] * m[7]) - (m[12] * m[3] * m[5]);

        inv[14] = -(m[0] * m[5] * m[14]) + (m[0] * m[6] * m[13]) + (m[4] * m[1] * m[14]) - (m[4] * m[2] * m[13]) -
                  (m[12] * m[1] * m[6]) + (m[12] * m[2] * m[5]);

        inv[3] = -(m[1] * m[6] * m[11]) + (m[1] * m[7] * m[10]) + (m[5] * m[2] * m[11]) - (m[5] * m[3] * m[10]) -
                 (m[9] * m[2] * m[7]) + (m[9] * m[3] * m[6]);

        inv[7] = (m[0] * m[6] * m[11]) - (m[0] * m[7] * m[10]) - (m[4] * m[2] * m[11]) + (m[4] * m[3] * m[10]) +
                 (m[8] * m[2] * m[7]) - (m[8] * m[3] * m[6]);

        inv[11] = -(m[0] * m[5] * m[11]) + (m[0] * m[7] * m[9]) + (m[4] * m[1] * m[11]) - (m[4] * m[3] * m[9]) -
                  (m[8] * m[1] * m[7]) + (m[8] * m[3] * m[5]);

        inv[15] = (m[0] * m[5] * m[10]) - (m[0] * m[6] * m[9]) - (m[4] * m[1] * m[10]) + (m[4] * m[2] * m[9]) +
                  (m[8] * m[1] * m[6]) - (m[8] * m[2] * m[5]);

        const f32 det = (m[0] * inv[0]) + (m[1] * inv[4]) + (m[2] * inv[8]) + (m[3] * inv[12]);

        if (std::abs(det) < epsilon) {
            return identity();
        }

        const f32 invDet = 1.0f / det;
        Mat4 result;
        for (usize i = 0; i < 16; ++i) {
            result.data()[i] = inv[i] * invDet;
        }
        return result;
    }
};

}  // namespace engine::core
