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
constexpr float kMinCellEms = 6.0F;
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

bool DrawChoice(settings::Registry& registry, const Item& item, const std::string& value) {
    bool isChosen = false;
    if (ImGui::BeginCombo(item.label, value.c_str())) {
        for (const std::string& choice : item.declaration.choices) {
            if (ImGui::Selectable(choice.c_str(), choice == value) && choice != value) {
                Assign(registry, item.key, choice);
                isChosen = true;
            }
        }
        ImGui::EndCombo();
    }
    return isChosen;
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

std::string TooltipOf(const settings::Registry::Declaration& declaration) {
    std::string tooltip = declaration.tooltip;
    if (declaration.changesGameplay) {
        tooltip += tooltip.empty() ? kGameplayTooltip : std::string("\n\n") + kGameplayTooltip;
    }
    return tooltip;
}

std::string_view LabelOf(std::string_view key, const settings::Registry::Declaration& declaration) {
    return declaration.label.empty() ? key : std::string_view(declaration.label);
}

void DrawItem(settings::Registry& registry, std::string_view key, std::string label,
              std::vector<std::string>& finished) {
    const settings::Registry::Declaration& declaration =
        registry.AllDeclarations().find(key)->second;
    if (declaration.changesGameplay) {
        label += " *";
    }
    const Item item{.key = key, .label = label.c_str(), .declaration = declaration};
    const settings::SettingValue value = registry.AllValues().find(key)->second;
    ImGui::PushID(key.data(), key.data() + key.size());
    ImGui::BeginDisabled(!registry.IsAvailable(key));
    const bool isFinished = std::visit(
        [&]<typename V>(const V& current) {
            if constexpr (std::same_as<V, bool>) {
                return DrawBool(registry, item, current);
            } else if constexpr (std::same_as<V, std::int64_t>) {
                return DrawInt(registry, item, current);
            } else if constexpr (std::same_as<V, double>) {
                return DrawDouble(registry, item, current);
            } else if (declaration.choices.empty()) {
                return DrawText(registry, item, current);
            } else {
                return DrawChoice(registry, item, current);
            }
        },
        value);
    ImGui::EndDisabled();
    const std::string tooltip = TooltipOf(declaration);
    if (!tooltip.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) {
        ImGui::SetTooltip("%s", tooltip.c_str());
    }
    ImGui::PopID();
    if (isFinished) {
        finished.emplace_back(key);
    }
}

void DrawItems(settings::Registry& registry, std::span<const std::string_view> keys,
               std::vector<std::string>& finished) {
    for (const std::string_view key : keys) {
        DrawItem(registry, key,
                 std::string(LabelOf(key, registry.AllDeclarations().find(key)->second)), finished);
    }
}

float CellWidth(const settings::Registry& registry, std::string_view column,
                std::span<const SettingGridRow> rows, std::size_t index) {
    float width = std::max(ImGui::CalcTextSize(column.data(), column.data() + column.size()).x,
                           ImGui::GetFontSize() * kMinCellEms);
    for (const SettingGridRow& row : rows) {
        const std::string_view key = row.keys[index];
        if (key.empty()) {
            continue;
        }
        if (const auto* text = std::get_if<std::string>(&registry.AllValues().find(key)->second)) {
            width = std::max(width, ImGui::CalcTextSize(text->c_str()).x);
        }
    }
    return width + (ImGui::GetStyle().FramePadding.x * 2.0F) + ImGui::GetFontSize();
}

void DrawGridTable(settings::Registry& registry, const SettingGrid& grid,
                   std::vector<std::string>& finished) {
    const auto columnCount = static_cast<int>(grid.columns.size() + 1);
    if (!ImGui::BeginTable("grid", columnCount,
                           ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
                               ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_NoHostExtendX)) {
        return;
    }
    ImGui::TableSetupColumn("");
    std::vector<float> widths;
    for (std::size_t index = 0; index < grid.columns.size(); ++index) {
        ImGui::TableSetupColumn(std::string(grid.columns[index]).c_str());
        widths.push_back(CellWidth(registry, grid.columns[index], grid.rows, index));
    }
    ImGui::TableHeadersRow();
    for (const SettingGridRow& row : grid.rows) {
        ImGui::TableNextRow();
        ImGui::TableNextColumn();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(row.label.data(), row.label.data() + row.label.size());
        for (std::size_t index = 0; index < row.keys.size(); ++index) {
            ImGui::TableNextColumn();
            if (!row.keys[index].empty()) {
                ImGui::SetNextItemWidth(widths[index]);
                DrawItem(registry, row.keys[index], "##cell", finished);
            }
        }
    }
    ImGui::EndTable();
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

SettingGrid GroupSettingGrid(const settings::Registry::Declarations& declarations,
                             const SettingMenu& menu) {
    SettingGrid grid;
    const std::size_t columnCount = menu.submenus.size();
    for (std::size_t index = 0; index < columnCount; ++index) {
        const SettingSubmenu& column = menu.submenus[index];
        grid.columns.push_back(column.category);
        for (const std::string_view key : column.keys) {
            const std::string_view label = LabelOf(key, declarations.find(key)->second);
            auto row = std::ranges::find(grid.rows, label, &SettingGridRow::label);
            if (row == grid.rows.end()) {
                row = grid.rows.insert(
                    grid.rows.end(),
                    {.label = label, .keys = std::vector<std::string_view>(columnCount)});
            }
            row->keys[index] = key;
        }
    }
    std::ranges::sort(grid.rows, {}, &SettingGridRow::label);
    return grid;
}

std::vector<std::string> DrawSettingGrid(settings::Registry& registry, const SettingMenu& menu) {
    std::vector<std::string> finished;
    DrawItems(registry, menu.keys, finished);
    const SettingGrid grid = GroupSettingGrid(registry.AllDeclarations(), menu);
    if (!grid.columns.empty()) {
        ImGui::Separator();
        DrawGridTable(registry, grid, finished);
    }
    return finished;
}

}
