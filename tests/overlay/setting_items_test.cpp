#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "overlay/headless_ui.hpp"
#include "overlay/setting_menus.hpp"

namespace rr::overlay {
namespace {

using settings::SettingSpec;

constexpr SettingSpec<int> kFpsCap{.key = "video.fps_cap",
                                   .defaultValue = 0,
                                   .range = settings::SettingRange<int>{.min = 0, .max = 360},
                                   .label = "Framerate cap",
                                   .category = "Video"};

TEST_CASE("dragging a slider changes the setting at once and finishes on release") {
    settings::Registry registry;
    registry.Declare(kFpsCap);
    const SettingMenu menu = GroupSettingMenus(registry.AllDeclarations()).front();
    HeadlessUi ui;
    CHECK(ui.Frame(registry, menu).empty());

    ui.MoveMouse(0.01F);
    CHECK(ui.Frame(registry, menu).empty());
    ImGui::GetIO().AddMouseButtonEvent(0, true);
    CHECK(ui.Frame(registry, menu).empty());
    ui.MoveMouse(0.5F);
    CHECK(ui.Frame(registry, menu).empty());

    CHECK(registry.Get(kFpsCap) > 100);
    CHECK(registry.Get(kFpsCap) < 260);
    ImGui::GetIO().AddMouseButtonEvent(0, false);
    CHECK(ui.Frame(registry, menu) == std::vector<std::string>{"video.fps_cap"});
}

}
}
