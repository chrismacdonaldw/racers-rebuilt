#pragma once

#include <span>
#include <vector>

#include "app/track_model.hpp"
#include "render/render_view.hpp"
#include "sim/hazard/hazards.hpp"
#include "sim/track/track_entities.hpp"
#include "sim/track/track_entity_sources.hpp"

namespace rr::app {

/// `sources` must outlive it.
class GhostTrail {
public:
    GhostTrail(std::span<const render::MeshDraw> trackDraws, std::span<const NamedDraws> named,
               const sim::TrackEntitySources& sources);

    void Append(const sim::Hazards& hazards, std::span<const sim::EntityClock> clocks,
                std::vector<render::MeshDraw>& draws) const;

private:
    void AppendCopies(std::size_t ghost, const sim::EntityClock& clock,
                      std::vector<render::MeshDraw>& draws) const;

    const sim::TrackEntitySources* sources_;
    std::vector<render::MeshDraw> model_;
};

}
