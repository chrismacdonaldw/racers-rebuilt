#include "overlay/racer_tools.hpp"

#include "core/assert.hpp"

#include <imgui.h>

namespace rr::overlay {

void DrawRacerTools(const RacerTools& tools) {
    if (tools.racers.empty() || !ImGui::BeginMenu("Racer tools")) {
        return;
    }
    RR_ASSERT(tools.selected != nullptr);
    for (std::size_t i = 0; i < tools.racers.size(); ++i) {
        if (ImGui::MenuItem(tools.racers[i].c_str(), nullptr, *tools.selected == i, true)) {
            *tools.selected = i;
        }
    }
    ImGui::Separator();
    for (const RacerActionGroup& group : tools.groups) {
        if (!ImGui::BeginMenu(group.name.c_str())) {
            continue;
        }
        for (const RacerAction& action : group.actions) {
            if (ImGui::MenuItem(action.name.c_str())) {
                action.run();
            }
        }
        ImGui::EndMenu();
    }
    ImGui::EndMenu();
}

}
