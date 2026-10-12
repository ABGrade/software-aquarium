#pragma once

#include <vector>
#include "math.hpp"

struct Vertex
{
    Vec3 pos;
    Vec3 normal;
    Vec3 color;
    float alpha{1.0f};
};

struct Triangle
{
    int a{}, b{}, c{};
    float sortKey{};
};

struct Mesh
{
    std::vector<Vertex> vertices;
    std::vector<Triangle> tris;
    float boundingRadius{};
};
