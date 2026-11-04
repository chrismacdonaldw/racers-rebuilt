#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "settings/registry.hpp"
#include "settings/setting_spec.hpp"

namespace rr::settings {

/// `resets` are category prefixes put back to their defaults before `values` apply.
struct SettingPreset {
    std::string label;
    std::string menu;
    std::vector<std::string> resets;
    std::vector<std::pair<std::string, SettingValue>> values;
};

/// True for `prefix` itself and its submenus, so `A` holds `A/B` but not `AB`.
[[nodiscard]] bool IsInCategory(std::string_view category, std::string_view prefix);

/// Returns the keys whose value changed.
[[nodiscard]] std::vector<std::string> ApplyPreset(Registry& registry, const SettingPreset& preset);

[[nodiscard]] bool IsPresetActive(const Registry& registry, const SettingPreset& preset);

}
