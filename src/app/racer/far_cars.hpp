#pragma once

#include <cstddef>
#include <span>

#include "app/far_models.hpp"
#include "app/racer/racer_grid.hpp"
#include "sim/render_snapshot.hpp"

namespace rr::app {

/// Squared, from the camera. The original draws a car and casts its shadow only within it.
inline constexpr float kCarRangeSquared = 250000.0F;

/// The grid's draws start at `firstDraw` in the scene's meshes, in slot order.
void AddGridCars(const RacerGrid& racers, std::span<const sim::EntitySnapshot> poses,
                 std::size_t firstDraw, FarModels& out);

}
