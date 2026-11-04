#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "app/far_models.hpp"
#include "app/track_model.hpp"
#include "sim/animation/figure_retarget.hpp"
#include "sim/track/track_entities.hpp"

namespace rr::app {

/// `sources` must outlive it, and its draws point at joint poses it owns.
class TrackEntityDraws {
public:
    TrackEntityDraws(const assets::NameMap<JointedModel>& models,
                     const sim::TrackEntitySources& sources);

    /// `clocks` holds one clock per entity, in the order of `sources`.
    void Update(std::span<const sim::EntityClock> clocks);

    void Scroll(float elapsedMs);

    [[nodiscard]] std::span<const render::MeshDraw> Draws() const { return draws_; }
    [[nodiscard]] std::span<const std::uint32_t> Slots() const { return slots_; }
    [[nodiscard]] DrawSpan SpanOf(std::size_t entity) const { return spans_[entity]; }

    /// The entities' draws start at `firstDraw` in the scene's meshes.
    void AddFarModels(std::size_t firstDraw, FarModels& out) const;

private:
    const sim::TrackEntitySources* sources_;
    std::vector<render::MeshDraw> draws_;
    std::vector<std::uint32_t> slots_;
    std::vector<DrawSpan> spans_;
    std::vector<ScrolledDraws> scrolled_;
    std::vector<std::vector<Transform>> poses_;
    sim::FigureRetargeter retargeter_;
};

}
