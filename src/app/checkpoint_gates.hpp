#pragma once

#include <vector>

#include "render/render_view.hpp"
#include "settings/setting_spec.hpp"
#include "sim/race/checkpoint_graph.hpp"
#include "sim/race/lap_tracker.hpp"

namespace rr::app {

inline constexpr settings::SettingSpec<bool> kShowCheckpoints{.key = "dev.show_checkpoints",
                                                              .defaultValue = false,
                                                              .label = "Show checkpoints",
                                                              .category = "Developer"};

/// Replaces `out` with engine-space gate frames, highlighting the last gate crossed and the next.
void DrawGates(const sim::CheckpointGraph& graph, const sim::LapTracker& laps,
               std::vector<render::DebugLine>& out);

}
