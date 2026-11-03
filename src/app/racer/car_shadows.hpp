#pragma once

#include <optional>
#include <span>
#include <vector>

#include "app/ground_decal.hpp"
#include "app/quad_shapes.hpp"
#include "app/racer/car_look.hpp"
#include "assets/track_loader.hpp"
#include "core/vector3.hpp"
#include "render/render_view.hpp"
#include "sim/physics/chassis.hpp"
#include "sim/render_snapshot.hpp"

namespace rr::app {

class CarShadows {
public:
    CarShadows(const assets::NameMap<render::Material>& materials,
               std::span<const sim::ChassisSpec> chassis);

    /// Spans run by slot except `cameras`, one per player; later slots go by the first camera.
    void Append(std::span<const sim::EntitySnapshot> poses,
                std::span<const sim::RacerSnapshot> racers, std::span<const CarLook> looks,
                std::span<const Vector3> cameras, GroundDecals& ground,
                std::vector<render::QuadDraw>& out) const;

private:
    struct Shadow {
        QuadLook look;
        float width = 0.0F;
        float length = 0.0F;
    };

    std::vector<std::optional<Shadow>> shadows_;
};

}
