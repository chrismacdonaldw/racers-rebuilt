#pragma once

#include <cstddef>
#include <span>

#include "sim/input_frame.hpp"
#include "sim/physics/racer_controls.hpp"
#include "sim/race/race_session.hpp"
#include "sim/render_snapshot.hpp"

namespace rr::app {

inline constexpr std::size_t kPlayerSlot = 0;

/// `controls` holds one entry per racer. The player's entry is replaced by `input`.
void StepRace(sim::RaceSession& session, const sim::InputFrame& input,
              std::span<sim::RacerControls> controls);

/// Replaces the racers in `out` and its state hash, leaving the camera alone.
void SnapshotRace(const sim::RaceSession& session, sim::RenderSnapshot& out);

}
