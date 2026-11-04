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

struct SettingGridRow {
    std::string_view label;
    std::vector<std::string_view> keys;
};

struct SettingGrid {
    std::vector<std::string_view> columns;
    std::vector<SettingGridRow> rows;
};

/// No category, no menu. Category `A/B` is submenu `B` of `A`. Views point into `declarations`.
[[nodiscard]] std::vector<SettingMenu> GroupSettingMenus(
    const settings::Registry::Declarations& declarations);

/// Columns are `menu`'s submenus and rows are labels sorted by name. A missing cell has no key.
[[nodiscard]] SettingGrid GroupSettingGrid(const settings::Registry::Declarations& declarations,
                                           const SettingMenu& menu);

/// Submenus come first. Edits apply at once. Returns the keys whose edit finished this frame.
[[nodiscard]] std::vector<std::string> DrawSettingItems(settings::Registry& registry,
                                                        const SettingMenu& menu);

/// Own settings come first, then one table of the submenus. Returns the keys whose edit finished.
[[nodiscard]] std::vector<std::string> DrawSettingGrid(settings::Registry& registry,
                                                       const SettingMenu& menu);

}
