#pragma once

#include <optional>
#include <span>
#include <vector>

#include "app/ground_decal.hpp"
#include "app/quad_shapes.hpp"
#include "app/racer/car_look.hpp"
#include "assets/track_loader.hpp"
#include "render/render_view.hpp"
#include "sim/physics/chassis.hpp"
#include "sim/render_snapshot.hpp"

namespace rr::app {

class CarShadows {
public:
    CarShadows(const assets::NameMap<render::Material>& materials,
               std::span<const sim::ChassisSpec> chassis);

    /// Each span holds one entry per slot, poses in engine space. Ghosts cast none.
    void Append(std::span<const sim::EntitySnapshot> poses,
                std::span<const sim::RacerSnapshot> racers, std::span<const CarLook> looks,
                GroundDecals& ground, std::vector<render::QuadDraw>& out) const;

private:
    struct Shadow {
        QuadLook look;
        float width = 0.0F;
        float length = 0.0F;
    };

    std::vector<std::optional<Shadow>> shadows_;
};

}
