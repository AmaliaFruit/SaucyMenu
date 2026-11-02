#pragma once
// Minimal Unity-compatible vector/quaternion math (left-handed, Y up, Z forward) so pose and hit-test
// maths run in C++ without a call into the game per operation. Matches UnityEngine conventions.

#include <cmath>

namespace saucy {

struct Vec3 {
    float x = 0, y = 0, z = 0;
    Vec3 operator+(Vec3 o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(Vec3 o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3 operator/(float s) const { return {x / s, y / s, z / s}; }
    float dot(Vec3 o) const { return x * o.x + y * o.y + z * o.z; }
    Vec3 cross(Vec3 o) const { return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x}; }
    float length() const { return std::sqrt(dot(*this)); }
    Vec3 normalized() const {
        float l = length();
        return l > 1e-6f ? *this / l : Vec3{0, 0, 0};
    }
    static Vec3 right() { return {1, 0, 0}; }
    static Vec3 up() { return {0, 1, 0}; }
    static Vec3 forward() { return {0, 0, 1}; }
};

struct Quat {
    float x = 0, y = 0, z = 0, w = 1;

    Quat operator*(Quat q) const {
        return {w * q.x + x * q.w + y * q.z - z * q.y, w * q.y + y * q.w + z * q.x - x * q.z,
                w * q.z + z * q.w + x * q.y - y * q.x, w * q.w - x * q.x - y * q.y - z * q.z};
    }

    Vec3 rotate(Vec3 v) const {
        Vec3 u{x, y, z};
        Vec3 t = u.cross(v) * 2.0f;
        return v + t * w + u.cross(t);
    }

    Quat inverse() const {
        float n = x * x + y * y + z * z + w * w;
        return n > 0 ? Quat{-x / n, -y / n, -z / n, w / n} : Quat{};
    }

    /// Same result as UnityEngine.Quaternion.LookRotation(forward, up).
    static Quat lookRotation(Vec3 forward, Vec3 up) {
        Vec3 f = forward.normalized();
        Vec3 r = up.cross(f).normalized();
        Vec3 u = f.cross(r);
        // Rotation matrix columns are r, u, f.
        float m00 = r.x, m01 = u.x, m02 = f.x;
        float m10 = r.y, m11 = u.y, m12 = f.y;
        float m20 = r.z, m21 = u.z, m22 = f.z;
        float trace = m00 + m11 + m22;
        Quat q;
        if (trace > 0) {
            float s = std::sqrt(trace + 1.0f) * 2.0f;
            q = {(m21 - m12) / s, (m02 - m20) / s, (m10 - m01) / s, 0.25f * s};
        } else if (m00 > m11 && m00 > m22) {
            float s = std::sqrt(1.0f + m00 - m11 - m22) * 2.0f;
            q = {0.25f * s, (m01 + m10) / s, (m02 + m20) / s, (m21 - m12) / s};
        } else if (m11 > m22) {
            float s = std::sqrt(1.0f + m11 - m00 - m22) * 2.0f;
            q = {(m01 + m10) / s, 0.25f * s, (m12 + m21) / s, (m02 - m20) / s};
        } else {
            float s = std::sqrt(1.0f + m22 - m00 - m11) * 2.0f;
            q = {(m02 + m20) / s, (m12 + m21) / s, 0.25f * s, (m10 - m01) / s};
        }
        return q;
    }
};

}  // namespace saucy
