#pragma once

#include <string>
#include <vector>

#include "overlay/setting_menus.hpp"

#include <imgui.h>

namespace rr::overlay {

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

    using Draw = std::vector<std::string> (*)(settings::Registry&, const SettingMenu&);

    std::vector<std::string> Frame(settings::Registry& registry, const SettingMenu& menu,
                                   Draw draw = DrawSettingItems) {
        ImGui::NewFrame();
        ImGui::SetNextWindowPos({0.0F, 0.0F});
        ImGui::SetNextWindowSize({400.0F, 200.0F});
        ImGui::Begin("menu");
        std::vector<std::string> finished = draw(registry, menu);
        itemMin_ = ImGui::GetItemRectMin();
        itemMax_ = ImGui::GetItemRectMax();
        ImGui::End();
        ImGui::Render();
        return finished;
    }

    void MoveMouse(float fraction, float heightFraction = 0.5F) const {
        ImGui::GetIO().AddMousePosEvent(itemMin_.x + ((itemMax_.x - itemMin_.x) * fraction),
                                        itemMin_.y + ((itemMax_.y - itemMin_.y) * heightFraction));
    }

private:
    ImVec2 itemMin_;
    ImVec2 itemMax_;
};

}
