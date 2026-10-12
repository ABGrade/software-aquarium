#pragma once

#include <vector>
#include "fish_mesh.hpp"

struct FishConfig
{
    FishType type{ FishType::Guppy };
    float scale{1.0f};
    Vec3 color{0.9f, 0.5f, 0.3f};
    int segments{16};
    Vec3 position{0,0,0};
    Vec3 velocity{28,0,0};
    float headingDeg{0.0f}; // yaw around Y for initial orientation
    float pitchDeg{0.0f};   // pitch around Z for vertical aim
    float speed{28.0f};
};

struct FishState
{
    FishConfig cfg;
    Mesh mesh;
    float radius{};
    float mass{};
};

class Simulation
{
public:
    Simulation();
    void setAquariumHalf(float half) { boxHalf_ = half; }
    void setWaterTop(float top) { waterTop_ = top; }
    void setFromConfigs(const std::vector<FishConfig>& configs);
    std::vector<FishState>& fishes() { return fishes_; }
    const std::vector<FishState>& fishes() const { return fishes_; }
    void rebuildFish(int idx);
    void update(float dt);

private:
    float boxHalf_{80.0f};
    float waterTop_{80.0f};
    std::vector<FishState> fishes_;
};
