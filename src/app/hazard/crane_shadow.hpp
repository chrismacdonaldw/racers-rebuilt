#pragma once

#include <cstddef>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

#include "app/ground_decal.hpp"
#include "app/quad_shapes.hpp"
#include "assets/track_loader.hpp"
#include "assets/vfs.hpp"
#include "core/vector3.hpp"
#include "render/render_view.hpp"
#include "render/renderer.hpp"
#include "sim/hazard/hazards.hpp"
#include "sim/track/track_entities.hpp"
#include "sim/track/track_entity_sources.hpp"

namespace rr::app {

/// Empty when `track` has no crane shadow material, as every track but the crane's.
[[nodiscard]] std::optional<QuadLook> LoadCraneShadowLook(render::Renderer& renderer,
                                                          const assets::Vfs& vfs,
                                                          assets::TrackLoader& loader,
                                                          std::string_view track);

/// `sources` must outlive it.
class CraneShadow {
public:
    CraneShadow(std::optional<QuadLook> look, const sim::TrackEntitySources& sources)
        : look_(look), sources_(&sources) {}

    /// `cameras` are in engine space. Empty: no limit.
    void Append(const sim::Hazards& hazards, std::span<const sim::EntityClock> clocks,
                std::span<const Vector3> cameras, GroundDecals& ground,
                std::vector<render::QuadDraw>& out) const;

private:
    void AppendUnder(std::size_t crane, const sim::EntityClock& clock,
                     std::span<const Vector3> cameras, GroundDecals& ground,
                     std::vector<render::QuadDraw>& out) const;

    std::optional<QuadLook> look_;
    const sim::TrackEntitySources* sources_;
};

}
