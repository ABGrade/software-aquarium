#pragma once

#include <cmath>

struct Vec2
{
    float x{}, y{};
    Vec2() = default;
    Vec2(float X, float Y) : x(X), y(Y) {}
    Vec2 operator+(const Vec2& v) const { return {x + v.x, y + v.y}; }
    Vec2 operator-(const Vec2& v) const { return {x - v.x, y - v.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
};

struct Vec3
{
    float x{}, y{}, z{};
    Vec3() = default;
    Vec3(float X, float Y, float Z) : x(X), y(Y), z(Z) {}
    Vec3 operator+(const Vec3& v) const { return {x + v.x, y + v.y, z + v.z}; }
    Vec3 operator-(const Vec3& v) const { return {x - v.x, y - v.y, z - v.z}; }
    Vec3 operator-() const { return { -x, -y, -z }; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3 operator/(float s) const { return {x / s, y / s, z / s}; }
    Vec3& operator+=(const Vec3& v) { x += v.x; y += v.y; z += v.z; return *this; }
    Vec3& operator-=(const Vec3& v) { x -= v.x; y -= v.y; z -= v.z; return *this; }
};

inline float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(const Vec3& a, const Vec3& b)
{
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline float length(const Vec3& v) { return std::sqrt(dot(v, v)); }
inline Vec3 normalize(const Vec3& v)
{
    float len = length(v);
    return len > 1e-6f ? v / len : Vec3{0, 0, 0};
}

struct Vec4
{
    float x{}, y{}, z{}, w{};
    Vec4() = default;
    Vec4(float X, float Y, float Z, float W) : x(X), y(Y), z(Z), w(W) {}
};

struct Mat4
{
    // Row-major 4x4
    float m[4][4]{};

    static Mat4 identity()
    {
        Mat4 r;
        r.m[0][0] = r.m[1][1] = r.m[2][2] = r.m[3][3] = 1.0f;
        return r;
    }

    static Mat4 perspective(float fovRad, float aspect, float znear, float zfar)
    {
        Mat4 p{};
        float f = 1.0f / std::tan(fovRad * 0.5f);
        p.m[0][0] = f / aspect;
        p.m[1][1] = f;
        p.m[2][2] = zfar / (zfar - znear);
        p.m[2][3] = 1.0f;
        p.m[3][2] = (-zfar * znear) / (zfar - znear);
        return p;
    }

    static Mat4 lookAt(const Vec3& eye, const Vec3& target, const Vec3& up)
    {
        Vec3 f = normalize(target - eye);
        Vec3 r = normalize(cross(up, f));
        Vec3 u = cross(f, r);

        Mat4 m = Mat4::identity();
        m.m[0][0] = r.x; m.m[0][1] = r.y; m.m[0][2] = r.z;
        m.m[1][0] = u.x; m.m[1][1] = u.y; m.m[1][2] = u.z;
        m.m[2][0] = f.x; m.m[2][1] = f.y; m.m[2][2] = f.z;
        m.m[3][0] = -dot(r, eye);
        m.m[3][1] = -dot(u, eye);
        m.m[3][2] = -dot(f, eye);
        return m;
    }
};

inline Vec4 mul(const Vec4& v, const Mat4& m)
{
    Vec4 r;
    r.x = v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0] + v.w * m.m[3][0];
    r.y = v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1] + v.w * m.m[3][1];
    r.z = v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2] + v.w * m.m[3][2];
    r.w = v.x * m.m[0][3] + v.y * m.m[1][3] + v.z * m.m[2][3] + v.w * m.m[3][3];
    return r;
}

inline Mat4 mul(const Mat4& a, const Mat4& b)
{
    Mat4 r{};
    for (int i = 0; i < 4; ++i)
    {
        for (int j = 0; j < 4; ++j)
        {
            r.m[i][j] = a.m[i][0] * b.m[0][j] + a.m[i][1] * b.m[1][j] + a.m[i][2] * b.m[2][j] + a.m[i][3] * b.m[3][j];
        }
    }
    return r;
}

inline Vec3 transformDir(const Vec3& v, const Mat4& m)
{
    return {
        v.x * m.m[0][0] + v.y * m.m[1][0] + v.z * m.m[2][0],
        v.x * m.m[0][1] + v.y * m.m[1][1] + v.z * m.m[2][1],
        v.x * m.m[0][2] + v.y * m.m[1][2] + v.z * m.m[2][2]
    };
}

inline Vec3 lerp(const Vec3& a, const Vec3& b, float t)
{
    return a * (1.0f - t) + b * t;
}
