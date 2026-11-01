#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "app/track_entity_draws.hpp"
#include "app/track_model.hpp"
#include "assets/event_table.hpp"
#include "render/renderer.hpp"
#include "sim/track/track_entity_sources.hpp"

namespace rr::app {

class HiddenModels {
public:
    HiddenModels(std::span<const assets::ModelDistanceResource> distances,
                 std::span<const NamedDraws> trackDraws, const sim::TrackEntitySources& sources);

    /// `draws` starts with the `track` draws and holds the `entities` draws from `entityOffset`.
    void Apply(const std::vector<bool>& hidden, std::span<const render::MeshDraw> track,
               const TrackEntityDraws& entities, std::size_t entityOffset,
               std::span<render::MeshDraw> draws) const;

private:
    struct Target {
        std::vector<DrawSpan> track;
        std::vector<std::size_t> entities;
    };

    std::vector<Target> targets_;
};

}
