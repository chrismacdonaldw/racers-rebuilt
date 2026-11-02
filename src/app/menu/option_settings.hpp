#pragma once

#include <vector>

#include "app/menu/options_menu.hpp"
#include "app/scene.hpp"
#include "settings/registry.hpp"

namespace rr::app {

/// A volume of 0 to 1 as one of the original's 21 slider steps.
[[nodiscard]] int VolumeStep(double volume);
[[nodiscard]] double StepVolume(int step);

/// The sound slider shows the effects volume.
[[nodiscard]] OptionValues ReadOptionValues(const settings::Registry& settings);

/// The sound slider sets the effects and voice volumes together.
[[nodiscard]] std::vector<SettingChange> OptionChanges(const OptionValues& before,
                                                       const OptionValues& after);

}
