#include "overlay/mod_menu.hpp"

#include <algorithm>
#include <format>
#include <utility>

#include <imgui.h>

namespace rr::overlay {
namespace {

constexpr const char* kQuestionDialog = "Mod changes";

}

std::vector<std::string> ModMenu::DrawRows(settings::Registry& registry,
                                           const mods::ModList& mods) {
    const std::vector<bool> toggles = mods.Toggles(registry);
    const mods::ModResolution resolution = mods.Resolve(registry);
    std::vector<std::string> changed;
    for (std::size_t index = 0; index < mods.All().size(); ++index) {
        const mods::ModManifest& manifest = mods.All()[index].Manifest();
        const std::string label = std::format("{}##{}", manifest.name, manifest.id);
        if (ImGui::MenuItem(label.c_str(), manifest.version.c_str(), toggles[index])) {
            mods::ModToggle toggle = mods::PlanModToggle(mods.All(), toggles, index);
            if (toggle.choices.size() == 1) {
                changed = mods.Apply(registry, toggle.choices.front().switches);
            } else {
                pending_ = std::move(toggle);
                opensQuestion_ = true;
            }
        }
        const auto held = std::ranges::find(resolution.held, index, &mods::HeldMod::index);
        if (held != resolution.held.end()) {
            ImGui::TextDisabled("  %s", held->reason.c_str());
        }
    }
    return changed;
}

std::vector<std::string> ModMenu::DrawQuestion(settings::Registry& registry,
                                               const mods::ModList& mods) {
    if (std::exchange(opensQuestion_, false)) {
        ImGui::OpenPopup(kQuestionDialog);
    }
    std::vector<std::string> changed;
    if (!pending_ ||
        !ImGui::BeginPopupModal(kQuestionDialog, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return changed;
    }
    ImGui::TextUnformatted(pending_->question.c_str());
    for (const mods::ModChoice& choice : pending_->choices) {
        if (&choice != &pending_->choices.front()) {
            ImGui::SameLine();
        }
        if (ImGui::Button(choice.label.c_str())) {
            changed = mods.Apply(registry, choice.switches);
            ImGui::CloseCurrentPopup();
        }
    }
    ImGui::EndPopup();
    if (!changed.empty() || !ImGui::IsPopupOpen(kQuestionDialog)) {
        pending_.reset();
    }
    return changed;
}

}
