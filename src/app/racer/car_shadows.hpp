#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "app/ground_decal.hpp"
#include "app/quad_shapes.hpp"
#include "app/racer/car_look.hpp"
#include "app/racer/car_silhouette.hpp"
#include "assets/track_loader.hpp"
#include "core/vector3.hpp"
#include "render/render_view.hpp"
#include "sim/render_snapshot.hpp"

namespace rr::app {

/// The original's alpha at a squared distance from the camera: full near, none far.
[[nodiscard]] std::uint8_t ShadowAlpha(float distanceSquared);

class CarShadows {
public:
    CarShadows(const assets::NameMap<render::Material>& materials,
               std::span<const ShadowCaster> casters);

    /// Slot spans; `cameras` has one per player, the first serving later slots. Empty: no fade.
    void Append(std::span<const sim::EntitySnapshot> poses,
                std::span<const sim::RacerSnapshot> racers, std::span<const CarLook> looks,
                std::span<const Vector3> cameras, GroundDecals& ground,
                std::vector<render::QuadDraw>& out) const;

private:
    struct Shadow {
        QuadLook look;
        float along = 0.0F;
        float across = 0.0F;
        Vector3 centre;
    };

    std::vector<std::optional<Shadow>> shadows_;
};

}
