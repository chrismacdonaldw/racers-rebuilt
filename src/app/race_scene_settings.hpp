#pragma once

#include "app/race_scene_name.hpp"
#include "settings/registry.hpp"
#include "sim/race/cheats.hpp"

namespace rr::app {

/// Fills the fields a race scene name leaves out, from `settings` and the `session` cheats.
void ApplyUnnamedSettings(RaceSceneSetup& setup, const settings::Registry& settings,
                          const sim::Cheats& session);

}
