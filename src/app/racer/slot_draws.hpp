#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "app/racer/racer_grid.hpp"
#include "app/track_model.hpp"
#include "render/render_view.hpp"

namespace rr::app {

/// The grid's draws start at `firstDraw` in the scene's meshes, in slot order.
[[nodiscard]] DrawSpan SlotDrawSpan(const RacerGrid& racers, std::size_t slot,
                                    std::size_t firstDraw);

/// Fills `copy`, which the result points into until `copy` changes.
[[nodiscard]] std::span<const render::MeshDraw> WithoutDraws(
    std::span<const render::MeshDraw> meshes, DrawSpan hidden, std::vector<render::MeshDraw>& copy);

}
