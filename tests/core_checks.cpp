#include "renderer.hpp"
#include "simulation.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>

void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}
bool close(float a, float b, float tolerance=0.001f) {
    return std::abs(a-b) <= tolerance * std::max(1.f, std::abs(b));
}
int main() {
    try {
        Renderer renderer(12,12,1);
        renderer.ambient={1,1,1};
        renderer.clear({0,0,0});
        VSOut a{}, b{}, c{};
        a.sx=1; a.sy=1; a.z=1; a.invZ=1; a.color={1,0,0};
        b.sx=9; b.sy=1; b.z=2; b.invZ=.5f; b.color={0,1,0};
        c.sx=1; c.sy=9; c.z=4; c.invZ=.25f; c.color={0,0,1};
        a.alpha=b.alpha=c.alpha=1; a.normal=b.normal=c.normal={0,0,1};
        renderer.drawTriangle(a,b,c,true,true);
        const auto pixel=renderer.resolve()[2*12+2];
        require(std::abs(int((pixel>>16)&255)-208)<=2 &&
                std::abs(int((pixel>>8)&255)-31)<=2 &&
                std::abs(int(pixel&255)-15)<=2,
                "Perspective interpolation assigns weights to the wrong vertices");

        Renderer ssaa(3,4,2);
        ssaa.clear({1,0,0});
        for (auto color:ssaa.resolve()) require((color&0xffffff)==0xff0000,"SSAA changed a uniform color");

        Simulation wall;
        wall.setAquariumHalf(200); wall.setWaterTop(200);
        FishConfig fish; fish.scale=.1f; fish.position={199,0,0}; fish.velocity={20,0,0};
        wall.setFromConfigs({fish}); wall.update(.1f);
        const auto& hit=wall.fishes()[0];
        require(hit.cfg.position.x<=200-hit.radius && hit.cfg.velocity.x<0,"Wall collision leaves fish outside aquarium");

        Simulation pair;
        pair.setAquariumHalf(1000); pair.setWaterTop(1000);
        FishConfig left,right; left.scale=.2f; right.scale=.3f;
        left.position={-1,0,0}; right.position={1,0,0};
        left.velocity={10,0,0}; right.velocity={-6,0,0};
        pair.setFromConfigs({left,right});
        const auto before=pair.fishes();
        float momentum=before[0].mass*10-before[1].mass*6;
        float energy=before[0].mass*100+before[1].mass*36;
        pair.update(0);
        const auto& after=pair.fishes();
        require(close(after[0].mass*after[0].cfg.velocity.x+after[1].mass*after[1].cfg.velocity.x,momentum),"Collision lost momentum");
        require(close(after[0].mass*dot(after[0].cfg.velocity,after[0].cfg.velocity)+after[1].mass*dot(after[1].cfg.velocity,after[1].cfg.velocity),energy),"Elastic collision lost kinetic energy");
        std::cout<<"Rendering, SSAA, wall collision and elastic collision checks passed\n";
        return 0;
    } catch(const std::exception& error) {
        std::cerr<<error.what()<<'\n'; return 1;
    }
}
