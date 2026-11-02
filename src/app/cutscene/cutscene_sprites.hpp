#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <vector>

#include "app/cutscene/cutscene_bindings.hpp"
#include "app/quad_shapes.hpp"
#include "assets/track_loader.hpp"
#include "core/result.hpp"
#include "core/transform.hpp"
#include "render/render_view.hpp"

namespace rr::app {

/// Draws the `WDB` sprites that indexed model events place, facing the camera.
class CutsceneSprites {
public:
    /// Fails on a sprite whose material `materials` lacks, as the original does.
    [[nodiscard]] static Result<CutsceneSprites> Create(
        std::span<const ModelBinding> models, const assets::NameMap<render::Material>& materials);

    /// `placement` is in game space and `camera` in engine space.
    void Append(std::size_t model, const Transform& placement, const Transform& camera,
                std::vector<render::QuadDraw>& out) const;

private:
    struct Sprite {
        QuadLook look;
        float width = 0.0F;
        float height = 0.0F;
        float maxDistance = 0.0F;
    };

    CutsceneSprites() = default;

    std::vector<std::optional<Sprite>> sprites_;
};

}
