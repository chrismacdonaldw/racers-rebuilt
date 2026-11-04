#pragma once

#include <cstddef>
#include <span>
#include <string>
#include <vector>

#include "mods/mod.hpp"

namespace rr::mods {

struct ModSwitch {
    std::size_t index;
    bool isOn;
};

struct ModChoice {
    std::string label;
    std::vector<ModSwitch> switches;
};

/// One choice applies at once. Several ask `question` first.
struct ModToggle {
    std::string question;
    std::vector<ModChoice> choices;
};

/// Turning a mod on turns on what it requires and asks about conflicts. Turning one off asks
/// before also turning off the mods that require it.
[[nodiscard]] ModToggle PlanModToggle(std::span<const Mod> mods, const std::vector<bool>& isOn,
                                      std::size_t index);

/// Turns on what each mod that's on requires, as turning it on would have.
[[nodiscard]] std::vector<ModSwitch> RequiredSwitches(std::span<const Mod> mods,
                                                      const std::vector<bool>& isOn);

}
