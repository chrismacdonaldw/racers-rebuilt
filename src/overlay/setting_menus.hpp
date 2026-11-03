#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "settings/registry.hpp"

namespace rr::overlay {

struct SettingSubmenu {
    std::string_view category;
    std::vector<std::string_view> keys;
};

struct SettingMenu {
    std::string_view category;
    std::vector<std::string_view> keys;
    std::vector<SettingSubmenu> submenus;
};

/// No category, no menu. Category `A/B` is submenu `B` of `A`. Views point into `declarations`.
[[nodiscard]] std::vector<SettingMenu> GroupSettingMenus(
    const settings::Registry::Declarations& declarations);

/// Submenus come first. Edits apply at once. Returns the keys whose edit finished this frame.
[[nodiscard]] std::vector<std::string> DrawSettingItems(settings::Registry& registry,
                                                        const SettingMenu& menu);

}
