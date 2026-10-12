#pragma once

#include <vector>
#include "math.hpp"

enum class FishType
{
    Guppy,
    Discus,
    Veerohvost,
    Barbus
};

inline const char* fishTypeName(FishType t)
{
    switch (t)
    {
    case FishType::Guppy: return "Guppy";
    case FishType::Discus: return "Discus";
    case FishType::Veerohvost: return "FanTail";
    case FishType::Barbus: return "Barbus";
    default: return "Unknown";
    }
}

inline std::vector<Vec2> profileFor(FishType type)
{
    switch (type)
    {
    case FishType::Guppy:
        return { {0,0}, {16,9}, {47,9}, {58,20}, {58,0} };
    case FishType::Discus:
        return { {0,0}, {29,29}, {53,6}, {58,16}, {58,0} };
    case FishType::Veerohvost:
        return { {0,0}, {9,4}, {20,15}, {35,15}, {45,5}, {58,29}, {58,0} };
    case FishType::Barbus:
        return { {0,0}, {33,18}, {64,18}, {85,7}, {102,24}, {91,0} };
    }
    return {};
}
