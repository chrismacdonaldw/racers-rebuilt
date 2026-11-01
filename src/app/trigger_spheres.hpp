#pragma once

#include <vector>

#include "render/render_view.hpp"
#include "settings/setting_spec.hpp"
#include "sim/track/track_events.hpp"

namespace rr::app {

inline constexpr settings::SettingSpec<bool> kShowTriggerSpheres{.key = "dev.show_triggers",
                                                                 .defaultValue = false,
                                                                 .label = "Show trigger spheres",
                                                                 .category = "Developer"};

/// Appends engine-space rings for every trigger sphere, lit while a racer is inside.
void DrawTriggerSpheres(const sim::TrackEvents& triggers, std::vector<render::DebugLine>& out);

}
