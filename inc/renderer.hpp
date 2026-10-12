#pragma once

#include <vector>
#include <cstdint>
#include "math.hpp"
#include "mesh.hpp"

struct ColorF
{
    float r{}, g{}, b{}, a{};
};

struct VSOut
{
    float sx{}, sy{}; // screen-space coords in supersampled buffer
    float z{};        // view-space z (positive forward)
    float invZ{};     // 1/z for perspective-correct interpolation
    Vec3 normal;
    Vec3 color;
    float alpha{};
};

class Renderer
{
public:
    Renderer(int w, int h, int ssaaFactor);

    void resize(int w, int h);
    void clear(const Vec3& color);

    // render triangles; assumes vertices already in screen space and inside frustum
    void drawTriangle(const VSOut& v0, const VSOut& v1, const VSOut& v2, bool depthWrite, bool depthTest);

    // resolve supersampled buffer to output 32-bit BGRA image
    const std::vector<uint32_t>& resolve();

    int width() const { return width_; }
    int height() const { return height_; }
    int internalWidth() const { return bufW_; }
    int internalHeight() const { return bufH_; }
    void setViewportWidth(int w);

    float nearPlane = 0.1f;
    float farPlane = 1000.0f;

    // helpers for converting world/view points to screen coordinates
    bool projectToScreen(const Vec3& viewPos, const Vec3& normal, const Vec3& color, float alpha, VSOut& out) const;

    // light parameters
    Vec3 ambient {0.3f, 0.3f, 0.3f};
    Vec3 lightDir { -0.3f, -0.7f, -0.6f };

private:
    int width_{}, height_{};
    int ssaa_{};
    int bufW_{}, bufH_{};
    int viewportW_{};
    std::vector<ColorF> colorBuffer_;
    std::vector<float> depthBuffer_;
    std::vector<uint32_t> resolved_;
};
