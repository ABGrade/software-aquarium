#include "simulation.hpp"
#include <algorithm>

Simulation::Simulation() = default;

void Simulation::setFromConfigs(const std::vector<FishConfig>& configs)
{
    fishes_.clear();
    fishes_.reserve(configs.size());
    for (size_t i = 0; i < configs.size(); ++i)
    {
        FishState fs;
        fs.cfg = configs[i];
        fishes_.push_back(fs);
        rebuildFish(static_cast<int>(i));
    }
}

void Simulation::rebuildFish(int idx)
{
    if (idx < 0 || idx >= static_cast<int>(fishes_.size())) return;
    auto& f = fishes_[idx];
    f.mesh = generateLatheFish(f.cfg.type, f.cfg.segments, f.cfg.scale);
    f.radius = f.mesh.boundingRadius;
    f.mass = f.radius * f.radius * f.radius; // proportional to volume
}

void Simulation::update(float dt)
{
    // integrate
    for (auto& f : fishes_)
    {
        f.cfg.position += f.cfg.velocity * dt;
        // collisions with box walls
        for (int axis = 0; axis < 3; ++axis)
        {
            float* pos = axis == 0 ? &f.cfg.position.x : (axis == 1 ? &f.cfg.position.y : &f.cfg.position.z);
            float* vel = axis == 0 ? &f.cfg.velocity.x : (axis == 1 ? &f.cfg.velocity.y : &f.cfg.velocity.z);
            float limitPos = boxHalf_ - f.radius;
            if (axis == 1)
                limitPos = std::min(limitPos, waterTop_ - f.radius);
            float limitNeg = -boxHalf_ + f.radius;
            if (*pos > limitPos)
            {
                *pos = limitPos;
                *vel = -*vel;
            }
            else if (*pos < limitNeg)
            {
                *pos = limitNeg;
                *vel = -*vel;
            }
        }
    }

    // fish-fish elastic collisions (sphere approx)
    for (size_t i = 0; i < fishes_.size(); ++i)
    {
        for (size_t j = i + 1; j < fishes_.size(); ++j)
        {
            auto& a = fishes_[i];
            auto& b = fishes_[j];
            Vec3 delta = a.cfg.position - b.cfg.position;
            float dist = length(delta);
            float minDist = a.radius + b.radius;
            if (dist < minDist && dist > 1e-4f)
            {
                Vec3 n = delta * (1.0f / dist);
                float relVel = dot(a.cfg.velocity - b.cfg.velocity, n);
                if (relVel < 0.0f)
                {
                    float m1 = a.mass, m2 = b.mass;
                    float p = (2.0f * relVel) / (m1 + m2);
                    a.cfg.velocity -= n * (p * m2);
                    b.cfg.velocity += n * (p * m1);
                }
                // positional correction to avoid sticking
                float penetration = minDist - dist;
                Vec3 correction = n * (penetration * 0.5f);
                a.cfg.position += correction;
                b.cfg.position -= correction;
            }
        }
    }
}
