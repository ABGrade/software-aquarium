#include "renderer.hpp"
#include <algorithm>
#include <cmath>
#include <thread>

Renderer::Renderer(int w, int h, int ssaaFactor)
{
    if (ssaaFactor < 1) ssaaFactor = 1;
    ssaa_ = ssaaFactor;
    resize(w, h);
    lightDir = normalize(lightDir);
}

void Renderer::setViewportWidth(int w)
{
    viewportW_ = std::max(1, w * ssaa_);
}

void Renderer::resize(int w, int h)
{
    width_ = std::max(1, w);
    height_ = std::max(1, h);
    bufW_ = width_ * ssaa_;
    bufH_ = height_ * ssaa_;
    viewportW_ = bufW_;
    colorBuffer_.assign(bufW_ * bufH_, {0,0,0,1});
    depthBuffer_.assign(bufW_ * bufH_, 1e9f);
    resolved_.assign(width_ * height_, 0);
}

void Renderer::clear(const Vec3& color)
{
    ColorF c{color.x, color.y, color.z, 1.0f};
    std::fill(colorBuffer_.begin(), colorBuffer_.end(), c);
    std::fill(depthBuffer_.begin(), depthBuffer_.end(), 1e9f);
}

bool Renderer::projectToScreen(const Vec3& viewPos, const Vec3& normal, const Vec3& color, float alpha, VSOut& out) const
{
    if (viewPos.z <= nearPlane) return false;
    float fov = 60.0f * 3.14159265f / 180.0f;
    float f = 1.0f / std::tan(fov * 0.5f);
    float aspect = static_cast<float>(viewportW_) / static_cast<float>(bufH_);

    float ndcX = (viewPos.x * f / aspect) / viewPos.z;
    float ndcY = (viewPos.y * f) / viewPos.z;
    if (ndcX < -1.5f || ndcX > 1.5f || ndcY < -1.5f || ndcY > 1.5f) return false; // coarse cull

    out.sx = (ndcX * 0.5f + 0.5f) * viewportW_;
    out.sy = (-ndcY * 0.5f + 0.5f) * bufH_;
    out.z = viewPos.z;
    out.invZ = 1.0f / viewPos.z;
    out.normal = normal;
    out.color = color;
    out.alpha = alpha;
    return true;
}

void Renderer::drawTriangle(const VSOut& v0, const VSOut& v1, const VSOut& v2, bool depthWrite, bool depthTest)
{
    // bounding box in integer pixel space
    float minXf = std::min({ v0.sx, v1.sx, v2.sx });
    float maxXf = std::max({ v0.sx, v1.sx, v2.sx });
    float minYf = std::min({ v0.sy, v1.sy, v2.sy });
    float maxYf = std::max({ v0.sy, v1.sy, v2.sy });
    int minX = static_cast<int>(std::floor(minXf));
    int maxX = static_cast<int>(std::ceil(maxXf));
    int minY = static_cast<int>(std::floor(minYf));
    int maxY = static_cast<int>(std::ceil(maxYf));
    if (maxX < 0 || maxY < 0 || minX >= bufW_ || minY >= bufH_) return;
    minX = std::max(0, minX);
    minY = std::max(0, minY);
    maxX = std::min(bufW_ - 1, maxX);
    maxY = std::min(bufH_ - 1, maxY);

    // edge functions
    float area = (v1.sx - v0.sx) * (v2.sy - v0.sy) - (v2.sx - v0.sx) * (v1.sy - v0.sy);
    if (std::fabs(area) < 1e-8f) return;
    float invArea = 1.0f / area;

    for (int y = minY; y <= maxY; ++y)
    {
        for (int x = minX; x <= maxX; ++x)
        {
            float px = x + 0.5f;
            float py = y + 0.5f;
            // Each barycentric weight belongs to the vertex opposite its edge.
            float w0 = (v2.sx - v1.sx) * (py - v1.sy) - (v2.sy - v1.sy) * (px - v1.sx);
            float w1 = (v0.sx - v2.sx) * (py - v2.sy) - (v0.sy - v2.sy) * (px - v2.sx);
            float w2 = (v1.sx - v0.sx) * (py - v0.sy) - (v1.sy - v0.sy) * (px - v0.sx);
            // Backface cull implicit with winding: require same sign as area
            if ((w0 >= 0 && w1 >= 0 && w2 >= 0 && area > 0) || (w0 <= 0 && w1 <= 0 && w2 <= 0 && area < 0))
            {
                w0 *= invArea; w1 *= invArea; w2 *= invArea;
                float invZ = v0.invZ * w0 + v1.invZ * w1 + v2.invZ * w2;
                if (invZ <= 0) continue;
                float z = 1.0f / invZ;
                int idx = y * bufW_ + x;
                if (depthTest && z >= depthBuffer_[idx]) continue;

                float lx = (v0.normal.x * v0.invZ * w0 + v1.normal.x * v1.invZ * w1 + v2.normal.x * v2.invZ * w2) * z;
                float ly = (v0.normal.y * v0.invZ * w0 + v1.normal.y * v1.invZ * w1 + v2.normal.y * v2.invZ * w2) * z;
                float lz = (v0.normal.z * v0.invZ * w0 + v1.normal.z * v1.invZ * w1 + v2.normal.z * v2.invZ * w2) * z;
                Vec3 n = normalize({ lx, ly, lz });
                float alpha = (v0.alpha * v0.invZ * w0 + v1.alpha * v1.invZ * w1 + v2.alpha * v2.invZ * w2) * z;
                Vec3 baseColor{
                    (v0.color.x * v0.invZ * w0 + v1.color.x * v1.invZ * w1 + v2.color.x * v2.invZ * w2) * z,
                    (v0.color.y * v0.invZ * w0 + v1.color.y * v1.invZ * w1 + v2.color.y * v2.invZ * w2) * z,
                    (v0.color.z * v0.invZ * w0 + v1.color.z * v1.invZ * w1 + v2.color.z * v2.invZ * w2) * z
                };

                float ndotl = std::max(0.0f, dot(n, -lightDir));
                float diff = 0.5f * ndotl + 0.5f; // half-Lambert to soften artifacts
                Vec3 shaded{
                    baseColor.x * (ambient.x + (1.0f - ambient.x) * diff),
                    baseColor.y * (ambient.y + (1.0f - ambient.y) * diff),
                    baseColor.z * (ambient.z + (1.0f - ambient.z) * diff)
                };
                // clamp
                shaded.x = std::min(1.0f, std::max(0.0f, shaded.x));
                shaded.y = std::min(1.0f, std::max(0.0f, shaded.y));
                shaded.z = std::min(1.0f, std::max(0.0f, shaded.z));
                alpha = std::min(1.0f, std::max(0.0f, alpha));

                ColorF& dst = colorBuffer_[idx];
                dst.r = shaded.x * alpha + dst.r * (1.0f - alpha);
                dst.g = shaded.y * alpha + dst.g * (1.0f - alpha);
                dst.b = shaded.z * alpha + dst.b * (1.0f - alpha);
                dst.a = alpha + dst.a * (1.0f - alpha);
                if (depthWrite) depthBuffer_[idx] = z;
            }
        }
    }
}

const std::vector<uint32_t>& Renderer::resolve()
{
    // downsample SSAA by averaging ssaa_*ssaa_ block
    int block = ssaa_;
    int threads = std::max(2u, std::thread::hardware_concurrency());
    int rowsPer = (height_ + threads - 1) / threads;
    std::vector<std::thread> pool;
    pool.reserve(threads);
    for (int t = 0; t < threads; ++t)
    {
        int yStart = t * rowsPer;
        int yEnd = std::min(height_, yStart + rowsPer);
        if (yStart >= yEnd) continue;
        pool.emplace_back([=]()
        {
            for (int y = yStart; y < yEnd; ++y)
            {
                for (int x = 0; x < width_; ++x)
                {
                    float r = 0, g = 0, b = 0;
                    for (int dy = 0; dy < block; ++dy)
                    {
                        int sy = y * block + dy;
                        int idx = sy * bufW_ + x * block;
                        for (int dx = 0; dx < block; ++dx)
                        {
                            const ColorF& c = colorBuffer_[idx + dx];
                            r += c.r; g += c.g; b += c.b;
                        }
                    }
                    float invCount = 1.0f / (block * block);
                    r *= invCount; g *= invCount; b *= invCount;
                    uint8_t R = static_cast<uint8_t>(std::min(1.0f, std::max(0.0f, r)) * 255.0f);
                    uint8_t G = static_cast<uint8_t>(std::min(1.0f, std::max(0.0f, g)) * 255.0f);
                    uint8_t B = static_cast<uint8_t>(std::min(1.0f, std::max(0.0f, b)) * 255.0f);
                    resolved_[y * width_ + x] = (0xFFu << 24) | (static_cast<uint32_t>(R) << 16) | (static_cast<uint32_t>(G) << 8) | static_cast<uint32_t>(B);
                }
            }
        });
    }
    for (auto& th : pool) th.join();
    return resolved_;
}
