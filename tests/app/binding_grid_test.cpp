#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <string_view>
#include <vector>

#include "app/binding_categories.hpp"
#include "app/input_bindings.hpp"
#include "overlay/setting_menus.hpp"

namespace rr::app {
namespace {

TEST_CASE("every binding gets its own cell on the Bindings screen") {
    settings::Registry registry;
    DeclareInputSettings(registry);
    const std::vector<overlay::SettingMenu> menus =
        overlay::GroupSettingMenus(registry.AllDeclarations());
    const auto bindings =
        std::ranges::find(menus, kBindingsCategory, &overlay::SettingMenu::category);
    REQUIRE(bindings != menus.end());

    const overlay::SettingGrid grid =
        overlay::GroupSettingGrid(registry.AllDeclarations(), *bindings);

    CHECK(grid.columns ==
          std::vector<std::string_view>{"Keys 1", "Keys 2", "Keys 3", "Pad 1", "Pad 2"});
    std::size_t declared = 0;
    for (const overlay::SettingSubmenu& column : bindings->submenus) {
        declared += column.keys.size();
    }
    std::size_t cells = 0;
    for (const overlay::SettingGridRow& row : grid.rows) {
        cells += static_cast<std::size_t>(
            std::ranges::count_if(row.keys, [](std::string_view key) { return !key.empty(); }));
    }
    CHECK(cells == declared);
}

}
}
