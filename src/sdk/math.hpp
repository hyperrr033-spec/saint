#pragma once
#include "../core/common.hpp"

namespace lacz {

inline constexpr float PI = 3.14159265358979323846f;
inline float D2R(float deg) { return deg * (PI / 180.f); }
inline float R2D(float rad) { return rad * (180.f / PI); }
inline float wrap_yaw(float yaw) {
    // fmod is O(1) and returns NaN for a NaN input, which the isfinite guard
    // below turns into 0. The old `while (yaw > 180) ...` loops spun forever on
    // inf/NaN (or on ~2^31 iterations for a huge finite value) and hard-froze
    // the game thread when a bad pitch/yaw reached them.
    if (!std::isfinite(yaw))
        return 0.f;
    if (yaw >= -180.f && yaw <= 180.f)
        return yaw;
    yaw = std::fmod(yaw + 180.f, 360.f);
    if (yaw < 0.f)
        yaw += 360.f;
    return yaw - 180.f;
}
inline float normalize_yaw(float yaw) { return wrap_yaw(yaw); }

struct vec2 {
    float x = 0.f, y = 0.f;
};

struct vec3 {
    float x = 0.f, y = 0.f, z = 0.f;

    vec3() = default;
    vec3(float v) : x(v), y(v), z(v) {}
    vec3(float a, float b, float c) : x(a), y(b), z(c) {}

    vec3 operator+(const vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    vec3 operator-(const vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    vec3 operator/(float s) const { return {x / s, y / s, z / s}; }
    vec3& operator+=(const vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    vec3& operator-=(const vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    bool operator==(const vec3& o) const { return x == o.x && y == o.y && z == o.z; }
    bool operator!=(const vec3& o) const { return !(*this == o); }

    float& operator[](int i) { return reinterpret_cast<float*>(this)[i]; }
    const float& operator[](int i) const { return reinterpret_cast<const float*>(this)[i]; }

    float length() const { return std::sqrt(x * x + y * y + z * z); }
    float length_2d() const { return std::sqrt(x * x + y * y); }
    vec3 normalized() const {
        float len = length();
        return len > 0.001f ? *this / len : vec3{};
    }
    bool is_zero() const { return x == 0.f && y == 0.f && z == 0.f; }
    float dot(const vec3& o) const { return x * o.x + y * o.y + z * o.z; }
    vec3 cross(const vec3& o) const {
        return {y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x};
    }
    // Distance ignoring vertical component (for 2D meters).
    float dist_2d(const vec3& o) const {
        float dx = x - o.x, dy = y - o.y;
        return std::sqrt(dx * dx + dy * dy);
    }
    float distance(const vec3& o) const { return (*this - o).length(); }
};

struct vec4 {
    float x = 0.f, y = 0.f, z = 0.f, w = 0.f;
    vec4() = default;
    vec4(float a, float b, float c, float d) : x(a), y(b), z(c), w(d) {}
};

// Valve's QAngle: pitch (x), yaw (y), roll (z), degrees.
struct ang {
    float pitch = 0.f, yaw = 0.f, roll = 0.f;

    ang() = default;
    ang(float p, float y, float r) : pitch(p), yaw(y), roll(r) {}

    ang operator+(const ang& o) const { return {pitch + o.pitch, yaw + o.yaw, roll + o.roll}; }
    ang operator-(const ang& o) const { return {pitch - o.pitch, yaw - o.yaw, roll - o.roll}; }
    ang operator*(float s) const { return {pitch * s, yaw * s, roll * s}; }
    ang operator/(float s) const { return {pitch / s, yaw / s, roll / s}; }

    void normalize() {
        // Branch-free and total: no while-loops, no NaN/inf spin. A pitch of
        // exactly 90 (or NaN) previously looped forever in the old code.
        if (!std::isfinite(pitch)) pitch = 0.f;
        if (!std::isfinite(yaw))   yaw = 0.f;
        pitch = std::fmod(pitch + 90.f, 180.f);
        if (pitch < 0.f) pitch += 180.f;
        pitch -= 90.f;
        yaw = wrap_yaw(yaw);
        roll = 0.f;
    }

    vec3 to_forward() const {
        float sp = std::sin(D2R(pitch)), cp = std::cos(D2R(pitch));
        float sy = std::sin(D2R(yaw)),   cy = std::cos(D2R(yaw));
        return {cp * cy, cp * sy, -sp};
    }
};

struct mat3x4 {
    float m[3][4];
};

// 4x4 matrix laid out row-major as Valve uses for world-to-screen.
struct matrix4x4 {
    float m[4][4];
};

struct view_matrix {
    float m[4][4];

    bool world_to_screen(const vec3& world, vec2& screen, int sw, int sh) const {
        float w = m[3][0] * world.x + m[3][1] * world.y + m[3][2] * world.z + m[3][3];
        if (w < 0.001f)
            return false;
        float x = m[0][0] * world.x + m[0][1] * world.y + m[0][2] * world.z + m[0][3];
        float y = m[1][0] * world.x + m[1][1] * world.y + m[1][2] * world.z + m[1][3];
        screen.x = (sw * 0.5f) + (x / w) * (sw * 0.5f);
        screen.y = (sh * 0.5f) - (y / w) * (sh * 0.5f);
        return true;
    }
};

} // namespace lacz