#include "settings/setting_preset.hpp"

#include <algorithm>
#include <string_view>

#include "core/log.hpp"

namespace rr::settings {
namespace {

bool IsReset(const SettingPreset& preset, std::string_view category) {
    return std::ranges::any_of(
        preset.resets, [&](const std::string& prefix) { return IsInCategory(category, prefix); });
}

bool HasValue(const SettingPreset& preset, std::string_view key) {
    return std::ranges::any_of(preset.values,
                               [&](const auto& value) { return value.first == key; });
}

bool Store(Registry& registry, std::string_view key, const SettingValue& value) {
    const SettingValue before = registry.AllValues().find(key)->second;
    const Result<void> assigned = registry.Assign(key, value);
    if (!assigned) {
        log::Warn("{}", assigned.error().message);
        return false;
    }
    return registry.AllValues().find(key)->second != before;
}

}

bool IsInCategory(std::string_view category, std::string_view prefix) {
    return category.starts_with(prefix) &&
           (category.size() == prefix.size() || category[prefix.size()] == '/');
}

std::vector<std::string> ApplyPreset(Registry& registry, const SettingPreset& preset) {
    std::vector<std::string> changed;
    for (const auto& [key, declaration] : registry.AllDeclarations()) {
        if (IsReset(preset, declaration.category) && !HasValue(preset, key) &&
            Store(registry, key, declaration.defaultValue)) {
            changed.push_back(key);
        }
    }
    for (const auto& [key, value] : preset.values) {
        if (!registry.AllDeclarations().contains(key)) {
            log::Warn("the {} preset names an unknown setting {}", preset.label, key);
            continue;
        }
        if (Store(registry, key, value)) {
            changed.push_back(key);
        }
    }
    return changed;
}

bool IsPresetActive(const Registry& registry, const SettingPreset& preset) {
    for (const auto& [key, declaration] : registry.AllDeclarations()) {
        if (IsReset(preset, declaration.category) && !HasValue(preset, key) &&
            registry.AllValues().find(key)->second != declaration.defaultValue) {
            return false;
        }
    }
    return std::ranges::all_of(preset.values, [&](const auto& value) {
        const auto current = registry.AllValues().find(value.first);
        return current != registry.AllValues().end() && current->second == value.second;
    });
}

}
