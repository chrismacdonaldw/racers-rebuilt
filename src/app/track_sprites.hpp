#pragma once

#include <optional>
#include <vector>

#include "app/quad_shapes.hpp"
#include "app/track_material_anims.hpp"
#include "assets/track_loader.hpp"
#include "assets/world_database.hpp"
#include "core/transform.hpp"
#include "core/vector3.hpp"
#include "render/render_view.hpp"

namespace rr::app {

/// The `WDB` sprites a track places, drawn as the original's world sprites are.
class TrackSprites {
public:
    /// Leaves out a sprite whose material `track`'s tables or `materials` lack.
    void Add(const assets::WorldAsset& world, const assets::TrackAssets& track,
             const assets::NameMap<render::Material>& materials);

    /// With `hidesFar`, leaves out each sprite past its draw distance from `camera`.
    void Append(const Transform& camera, bool hidesFar, float shownMs,
                std::vector<render::QuadDraw>& out) const;

private:
    struct Sprite {
        QuadLook look;
        std::optional<PlayedTrack> animation;
        Vector3 centre;
        std::optional<Vector3> top;
        float width = 0.0F;
        float height = 0.0F;
        float rangeSquared = 0.0F;
    };

    std::vector<Sprite> sprites_;
};

}
