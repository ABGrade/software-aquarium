#pragma once

#include "mesh.hpp"
#include "fish_profiles.hpp"
#include <cmath>
#include <algorithm>

inline Mesh generateLatheFish(FishType type, int segments, float scale)
{
    Mesh mesh;
    if (segments < 6) segments = 6;
    auto prof = profileFor(type);
    // nose at (0,0). Scale relative to nose.
    for (auto& p : prof)
    {
        p.x *= scale;
        p.y *= scale;
    }

    int rings = static_cast<int>(prof.size());
    mesh.vertices.reserve(rings * segments);
    const float twoPi = 6.28318530718f;
    for (int i = 0; i < rings; ++i)
    {
        // compute tangent to profile for normal
        Vec2 prev = prof[std::max(0, i - 1)];
        Vec2 next = prof[std::min(rings - 1, i + 1)];
        Vec2 tangent = { next.x - prev.x, next.y - prev.y };
        Vec2 n2 = { -tangent.y, tangent.x }; // outward normal in profile plane
        float len2 = std::sqrt(n2.x * n2.x + n2.y * n2.y);
        if (len2 < 1e-6f) { n2 = {0,1}; len2 = 1.0f; }
        n2 = n2 * (1.0f / len2);

        for (int j = 0; j < segments; ++j)
        {
            float ang = (j / static_cast<float>(segments)) * twoPi;
            float c = std::cos(ang), s = std::sin(ang);
            float x = prof[i].x;
            float y = prof[i].y * c;
            float z = prof[i].y * s;
            Vec3 normal3 = { n2.x, n2.y * c, n2.y * s };
            mesh.vertices.push_back({ {x, y, z}, normalize(normal3), {1,1,1}, 1.0f });
        }
    }

    // build triangles
    for (int i = 0; i < rings - 1; ++i)
    {
        for (int j = 0; j < segments; ++j)
        {
            int nextJ = (j + 1) % segments;
            int a = i * segments + j;
            int b = i * segments + nextJ;
            int c = (i + 1) * segments + j;
            int d = (i + 1) * segments + nextJ;

            mesh.tris.push_back({ a, c, b });
            mesh.tris.push_back({ b, c, d });
        }
    }

    // bounding radius
    float maxR = 0.0f;
    for (auto& v : mesh.vertices)
    {
        float r = length(v.pos);
        if (r > maxR) maxR = r;
    }
    mesh.boundingRadius = maxR;
    return mesh;
}
