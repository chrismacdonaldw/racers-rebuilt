#include "overlay/quick_config_menu.hpp"

#include <algorithm>
#include <format>
#include <iterator>
#include <span>
#include <string>
#include <utility>

#include "core/log.hpp"
#include "settings/setting_preset.hpp"

#include <imgui.h>

namespace rr::overlay {
namespace {

constexpr const char* kSaveDialog = "Save quick config";
constexpr const char* kDeleteDialog = "Delete quick config";

std::string ItemLabel(const settings::UserQuickConfig& config) {
    return std::format("{}##{}", config.preset.label, config.slug);
}

void Apply(settings::Registry& registry, const settings::SettingPreset& preset,
           const std::string& label, std::vector<std::string>& changed) {
    if (ImGui::MenuItem(label.c_str(), nullptr, settings::IsPresetActive(registry, preset))) {
        std::ranges::move(settings::ApplyPreset(registry, preset), std::back_inserter(changed));
    }
}

}

std::vector<std::string> QuickConfigMenu::DrawMenu(settings::Registry& registry,
                                                   std::string_view menu) {
    std::vector<std::string> changed;
    const auto isListed = [&](const settings::SettingPreset& preset) {
        return preset.menu == menu;
    };
    if (std::ranges::none_of(configs_.Shipped(), isListed) || !ImGui::BeginMenu("Presets")) {
        return changed;
    }
    for (const settings::SettingPreset& preset : configs_.Shipped()) {
        if (isListed(preset)) {
            Apply(registry, preset, preset.label, changed);
        }
    }
    const std::span<const settings::UserQuickConfig> user = configs_.User();
    if (!user.empty()) {
        ImGui::Separator();
    }
    for (const settings::UserQuickConfig& config : user) {
        Apply(registry, config.preset, ItemLabel(config), changed);
    }
    ImGui::Separator();
    if (ImGui::MenuItem("Save current as...")) {
        opensSave_ = true;
        name_.fill('\0');
        error_.clear();
    }
    if (!user.empty() && ImGui::BeginMenu("Delete")) {
        for (const settings::UserQuickConfig& config : user) {
            if (ImGui::MenuItem(ItemLabel(config).c_str())) {
                opensDelete_ = true;
                pendingDelete_ = config.slug;
                error_.clear();
            }
        }
        ImGui::EndMenu();
    }
    ImGui::EndMenu();
    return changed;
}

void QuickConfigMenu::DrawDialogs(const settings::Registry& registry) {
    if (std::exchange(opensSave_, false)) {
        ImGui::OpenPopup(kSaveDialog);
    }
    if (std::exchange(opensDelete_, false)) {
        ImGui::OpenPopup(kDeleteDialog);
    }
    DrawSave(registry);
    DrawDelete(registry);
}

void QuickConfigMenu::DrawSave(const settings::Registry& registry) {
    if (!ImGui::BeginPopupModal(kSaveDialog, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    ImGui::InputText("Name", name_.data(), name_.size());
    const std::string name(name_.data());
    const settings::UserQuickConfig* existing = configs_.Find(name);
    if (existing != nullptr) {
        ImGui::TextDisabled("Replaces %s", existing->preset.label.c_str());
    }
    if (!error_.empty()) {
        ImGui::TextUnformatted(error_.c_str());
    }
    ImGui::BeginDisabled(name.empty());
    if (ImGui::Button(existing != nullptr ? "Replace" : "Save")) {
        if (Result<void> saved = configs_.Save(registry, name); saved) {
            ImGui::CloseCurrentPopup();
        } else {
            log::Warn("{}", saved.error().message);
            error_ = saved.error().message;
        }
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

void QuickConfigMenu::DrawDelete(const settings::Registry& registry) {
    if (!ImGui::BeginPopupModal(kDeleteDialog, nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        return;
    }
    const auto found =
        std::ranges::find(configs_.User(), pendingDelete_, &settings::UserQuickConfig::slug);
    if (found == configs_.User().end()) {
        ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
        return;
    }
    ImGui::Text("Delete %s?", found->preset.label.c_str());
    if (!error_.empty()) {
        ImGui::TextUnformatted(error_.c_str());
    }
    if (ImGui::Button("Delete")) {
        if (Result<void> removed = configs_.Remove(registry, pendingDelete_); removed) {
            ImGui::CloseCurrentPopup();
        } else {
            log::Warn("{}", removed.error().message);
            error_ = removed.error().message;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) {
        ImGui::CloseCurrentPopup();
    }
    ImGui::EndPopup();
}

}
