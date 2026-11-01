#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "app/track_model.hpp"
#include "sim/track/track_entities.hpp"

namespace rr::app {

/// `sources` must outlive it.
class TrackEntityDraws {
public:
    TrackEntityDraws(const assets::NameMap<JointedModel>& models,
                     const sim::TrackEntitySources& sources);

    /// `clocks` holds one clock per entity, in the order of `sources`.
    void Update(std::span<const sim::EntityClock> clocks);

    [[nodiscard]] std::span<const render::MeshDraw> Draws() const { return draws_; }
    [[nodiscard]] DrawSpan SpanOf(std::size_t entity) const { return spans_[entity]; }

private:
    struct Part {
        std::size_t entity = 0;
        std::uint16_t joint = 0;
    };

    const sim::TrackEntitySources* sources_;
    std::vector<render::MeshDraw> draws_;
    std::vector<Part> parts_;
    std::vector<DrawSpan> spans_;
};

}
