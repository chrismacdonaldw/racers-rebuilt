#include "overlay/setting_menus.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <utility>
#include <variant>

#include "core/log.hpp"

#include <imgui.h>

namespace rr::overlay {
namespace {

constexpr std::size_t kMaxTextBytes = 512;
constexpr const char* kGameplayTooltip =
    "Changes gameplay. Replays record it and warn when played back without it.";

struct Item {
    std::string_view key;
    const char* label;
    const settings::Registry::Declaration& declaration;
};

void Assign(settings::Registry& registry, std::string_view key, settings::SettingValue value) {
    const Result<void> assigned = registry.Assign(key, std::move(value));
    if (!assigned) {
        log::Warn("{}", assigned.error().message);
    }
}

bool DrawBool(settings::Registry& registry, const Item& item, bool value) {
    if (ImGui::MenuItem(item.label, nullptr, value)) {
        Assign(registry, item.key, !value);
        return true;
    }
    return false;
}

bool DrawInt(settings::Registry& registry, const Item& item, std::int64_t value) {
    int edited = static_cast<int>(std::clamp<std::int64_t>(value, std::numeric_limits<int>::min(),
                                                           std::numeric_limits<int>::max()));
    const auto& range = item.declaration.range;
    const bool isChanged =
        range ? ImGui::SliderInt(item.label, &edited,
                                 static_cast<int>(std::get<std::int64_t>(range->first)),
                                 static_cast<int>(std::get<std::int64_t>(range->second)))
              : ImGui::InputInt(item.label, &edited);
    if (isChanged) {
        Assign(registry, item.key, std::int64_t{edited});
    }
    return ImGui::IsItemDeactivatedAfterEdit();
}

bool DrawDouble(settings::Registry& registry, const Item& item, double value) {
    auto edited = static_cast<float>(value);
    const auto& range = item.declaration.range;
    const bool isChanged =
        range ? ImGui::SliderFloat(item.label, &edited,
                                   static_cast<float>(std::get<double>(range->first)),
                                   static_cast<float>(std::get<double>(range->second)))
              : ImGui::InputFloat(item.label, &edited);
    if (isChanged) {
        Assign(registry, item.key, static_cast<double>(edited));
    }
    return ImGui::IsItemDeactivatedAfterEdit();
}

bool DrawText(settings::Registry& registry, const Item& item, const std::string& value) {
    std::array<char, kMaxTextBytes> buffer{};
    value.copy(buffer.data(), buffer.size() - 1);
    if (ImGui::InputText(item.label, buffer.data(), buffer.size(),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
        Assign(registry, item.key, std::string(buffer.data()));
        return true;
    }
    return false;
}

template <typename Menu>
Menu& FindOrAddMenu(std::vector<Menu>& menus, std::string_view category) {
    auto menu = std::ranges::find(menus, category, &Menu::category);
    if (menu == menus.end()) {
        Menu added;
        added.category = category;
        menu = menus.insert(menus.end(), std::move(added));
    }
    return *menu;
}

void DrawItems(settings::Registry& registry, std::span<const std::string_view> keys,
               std::vector<std::string>& finished) {
    for (const std::string_view key : keys) {
        const settings::Registry::Declaration& declaration =
            registry.AllDeclarations().find(key)->second;
        std::string label = declaration.label.empty() ? std::string(key) : declaration.label;
        if (declaration.changesGameplay) {
            label += " *";
        }
        const Item item{.key = key, .label = label.c_str(), .declaration = declaration};
        const settings::SettingValue value = registry.AllValues().find(key)->second;
        ImGui::PushID(key.data(), key.data() + key.size());
        const bool isFinished = std::visit(
            [&]<typename V>(const V& current) {
                if constexpr (std::same_as<V, bool>) {
                    return DrawBool(registry, item, current);
                } else if constexpr (std::same_as<V, std::int64_t>) {
                    return DrawInt(registry, item, current);
                } else if constexpr (std::same_as<V, double>) {
                    return DrawDouble(registry, item, current);
                } else {
                    return DrawText(registry, item, current);
                }
            },
            value);
        if (declaration.changesGameplay && ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", kGameplayTooltip);
        }
        ImGui::PopID();
        if (isFinished) {
            finished.emplace_back(key);
        }
    }
}

}

std::vector<SettingMenu> GroupSettingMenus(const settings::Registry::Declarations& declarations) {
    std::vector<SettingMenu> menus;
    for (const auto& [key, declaration] : declarations) {
        const std::string_view category = declaration.category;
        if (category.empty()) {
            continue;
        }
        const std::size_t slash = category.find('/');
        SettingMenu& menu = FindOrAddMenu(menus, category.substr(0, slash));
        if (slash == std::string_view::npos) {
            menu.keys.emplace_back(key);
        } else {
            FindOrAddMenu(menu.submenus, category.substr(slash + 1)).keys.emplace_back(key);
        }
    }
    std::ranges::sort(menus, {}, &SettingMenu::category);
    for (SettingMenu& menu : menus) {
        std::ranges::sort(menu.submenus, {}, &SettingSubmenu::category);
    }
    return menus;
}

std::vector<std::string> DrawSettingItems(settings::Registry& registry, const SettingMenu& menu) {
    std::vector<std::string> finished;
    for (const SettingSubmenu& submenu : menu.submenus) {
        if (ImGui::BeginMenu(std::string(submenu.category).c_str())) {
            DrawItems(registry, submenu.keys, finished);
            ImGui::EndMenu();
        }
    }
    DrawItems(registry, menu.keys, finished);
    return finished;
}

}
