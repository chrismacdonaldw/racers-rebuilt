#pragma once

#include <string_view>
#include <vector>

#include "assets/vfs.hpp"
#include "core/result.hpp"
#include "render/render_view.hpp"
#include "settings/setting_spec.hpp"
#include "sim/race/checkpoint_graph.hpp"
#include "sim/race/lap_tracker.hpp"

namespace rr::app {

inline constexpr settings::SettingSpec<bool> kShowCheckpoints{.key = "dev.show_checkpoints",
                                                              .defaultValue = false,
                                                              .label = "Show checkpoints",
                                                              .category = "Developer"};

[[nodiscard]] Result<sim::CheckpointGraph> LoadCheckpoints(const assets::Vfs& vfs,
                                                           std::string_view folder);

/// Replaces `out` with engine-space gate frames, highlighting the last gate crossed and the next.
void DrawGates(const sim::CheckpointGraph& graph, const sim::LapTracker& laps,
               std::vector<render::DebugLine>& out);

}
