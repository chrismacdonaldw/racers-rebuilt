#pragma once

#include <optional>
#include <span>
#include <vector>

#include "app/quad_shapes.hpp"
#include "assets/track_loader.hpp"
#include "assets/world_database.hpp"
#include "core/transform.hpp"
#include "core/vector3.hpp"
#include "render/render_view.hpp"

namespace rr::app {

/// The `WDB` sprites a track places, drawn as the original's world sprites are.
class TrackSprites {
public:
    /// Leaves out a sprite with a table material, or whose named material `materials` lacks.
    void Add(std::span<const assets::WorldSprite> sprites,
             const assets::NameMap<render::Material>& materials);

    /// With `hidesFar`, leaves out each sprite past its draw distance from `camera`.
    void Append(const Transform& camera, bool hidesFar, std::vector<render::QuadDraw>& out) const;

private:
    struct Sprite {
        QuadLook look;
        Vector3 centre;
        std::optional<Vector3> top;
        float width = 0.0F;
        float height = 0.0F;
        float rangeSquared = 0.0F;
    };

    std::vector<Sprite> sprites_;
};

}
