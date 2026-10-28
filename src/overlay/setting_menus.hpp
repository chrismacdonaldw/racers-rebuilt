#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "settings/registry.hpp"

namespace rr::overlay {

struct SettingMenu {
    std::string_view category;
    std::vector<std::string_view> keys;
};

/// Settings without a category get no menu. Views point into `declarations`.
[[nodiscard]] std::vector<SettingMenu> GroupSettingMenus(
    const settings::Registry::Declarations& declarations);

/// Edits apply at once. Returns the keys whose edit finished this frame, to persist.
[[nodiscard]] std::vector<std::string> DrawSettingItems(settings::Registry& registry,
                                                        const SettingMenu& menu);

}
