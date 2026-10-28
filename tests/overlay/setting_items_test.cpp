#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "overlay/setting_menus.hpp"

#include <imgui.h>

namespace rr::overlay {
namespace {

using settings::SettingSpec;

constexpr SettingSpec<int> kFpsCap{.key = "video.fps_cap",
                                   .defaultValue = 0,
                                   .range = settings::SettingRange<int>{.min = 0, .max = 360},
                                   .label = "Framerate cap",
                                   .category = "Video"};

class HeadlessUi {
public:
    HeadlessUi() {
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.DisplaySize = {800.0F, 600.0F};
        io.DeltaTime = 1.0F / 60.0F;
        io.Fonts->Build();
    }
    HeadlessUi(const HeadlessUi&) = delete;
    HeadlessUi& operator=(const HeadlessUi&) = delete;
    HeadlessUi(HeadlessUi&&) = delete;
    HeadlessUi& operator=(HeadlessUi&&) = delete;
    ~HeadlessUi() { ImGui::DestroyContext(); }

    std::vector<std::string> Frame(settings::Registry& registry, const SettingMenu& menu) {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0.0F, 0.0F});
        ImGui::SetNextWindowSize({400.0F, 200.0F});
        ImGui::Begin("menu");
        std::vector<std::string> finished = DrawSettingItems(registry, menu);
        sliderMin_ = ImGui::GetItemRectMin();
        sliderMax_ = ImGui::GetItemRectMax();
        ImGui::End();
        ImGui::Render();
        return finished;
    }

    void MoveMouse(float fraction) const {
        ImGui::GetIO().AddMousePosEvent(sliderMin_.x + ((sliderMax_.x - sliderMin_.x) * fraction),
                                        (sliderMin_.y + sliderMax_.y) / 2.0F);
    }

private:
    ImVec2 sliderMin_;
    ImVec2 sliderMax_;
};

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
