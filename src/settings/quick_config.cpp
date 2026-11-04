#include "settings/quick_config.hpp"

#include <algorithm>
#include <cstdint>
#include <optional>
#include <utility>
#include <variant>

#include "core/log.hpp"
#include "core/utf8_path.hpp"
#include "settings/toml_file.hpp"

namespace rr::settings {
namespace {

constexpr std::string_view kNameKey = "name";
constexpr std::string_view kSettingsKey = "settings";
constexpr std::string_view kModsCategory = "Mods";
constexpr std::string_view kFallbackSlug = "config";

bool IsNamedBy(std::span<const SettingPreset> presets, std::string_view key) {
    return std::ranges::any_of(presets, [&](const SettingPreset& preset) {
        return std::ranges::any_of(preset.values,
                                   [&](const auto& value) { return value.first == key; });
    });
}

bool IsResetBy(std::span<const SettingPreset> presets, std::string_view category) {
    return std::ranges::any_of(presets, [&](const SettingPreset& preset) {
        return std::ranges::any_of(preset.resets, [&](const std::string& prefix) {
            return IsInCategory(category, prefix);
        });
    });
}

std::optional<SettingValue> Fitted(const Registry::Declaration& declaration, SettingValue value) {
    if (std::holds_alternative<double>(declaration.defaultValue)) {
        if (const auto* whole = std::get_if<std::int64_t>(&value)) {
            value = static_cast<double>(*whole);
        }
    }
    if (value.index() != declaration.defaultValue.index()) {
        return std::nullopt;
    }
    if (declaration.range) {
        value = std::clamp(value, declaration.range->first, declaration.range->second);
    }
    return value;
}

}

Result<QuickConfig> ReadQuickConfig(const std::filesystem::path& path) {
    Result<TomlFile> file = ReadTomlFile(path);
    if (!file) {
        return std::unexpected(std::move(file.error()));
    }
    QuickConfig config{.name = file->table[kNameKey].value_or(Utf8FromPath(path.stem())),
                       .values = {}};
    if (const toml::table* settings = file->table[kSettingsKey].as_table()) {
        for (TomlSetting& setting : FlattenSettings(*settings)) {
            if (setting.value) {
                config.values.emplace_back(std::move(setting.key), std::move(*setting.value));
            }
        }
    }
    std::ranges::sort(config.values, {}, &std::pair<std::string, SettingValue>::first);
    return config;
}

Result<void> WriteQuickConfig(const QuickConfig& config, const std::filesystem::path& path) {
    toml::table table;
    table.insert(kNameKey, config.name);
    table.insert(kSettingsKey,
                 NestSettings(Registry::Values(config.values.begin(), config.values.end())));
    return WriteTomlFile({.table = std::move(table), .leadingComments = {}}, path);
}

std::string QuickConfigSlug(std::string_view name) {
    std::string slug;
    for (const char letter : name) {
        const bool isDigit = letter >= '0' && letter <= '9';
        const bool isLower = letter >= 'a' && letter <= 'z';
        const bool isUpper = letter >= 'A' && letter <= 'Z';
        if (isDigit || isLower || isUpper) {
            slug += isUpper ? static_cast<char>(letter - 'A' + 'a') : letter;
        } else if (!slug.empty() && slug.back() != '-') {
            slug += '-';
        }
    }
    while (!slug.empty() && slug.back() == '-') {
        slug.pop_back();
    }
    return slug.empty() ? std::string(kFallbackSlug) : slug;
}

std::vector<std::string> QuickConfigScope(const Registry& registry,
                                          std::span<const SettingPreset> shipped) {
    std::vector<std::string> scope;
    for (const auto& [key, declaration] : registry.AllDeclarations()) {
        if (IsResetBy(shipped, declaration.category) || IsNamedBy(shipped, key) ||
            IsInCategory(declaration.category, kModsCategory)) {
            scope.push_back(key);
        }
    }
    return scope;
}

QuickConfig SnapshotQuickConfig(const Registry& registry, std::span<const std::string> scope,
                                std::string name) {
    QuickConfig config{.name = std::move(name), .values = {}};
    for (const std::string& key : scope) {
        config.values.emplace_back(key, registry.AllValues().find(key)->second);
    }
    return config;
}

SettingPreset QuickConfigPreset(const QuickConfig& config, const Registry& registry,
                                std::span<const std::string> scope, std::string menu) {
    SettingPreset preset{.label = config.name, .menu = std::move(menu), .resets = {}, .values = {}};
    std::size_t ignored = 0;
    for (const auto& [key, value] : config.values) {
        ignored += std::ranges::contains(scope, key) ? 0 : 1;
    }
    for (const std::string& key : scope) {
        const Registry::Declaration& declaration = registry.AllDeclarations().find(key)->second;
        const auto stored =
            std::ranges::find(config.values, key, &std::pair<std::string, SettingValue>::first);
        std::optional<SettingValue> value = stored == config.values.end()
                                                ? declaration.defaultValue
                                                : Fitted(declaration, stored->second);
        if (!value) {
            ++ignored;
            value = declaration.defaultValue;
        }
        preset.values.emplace_back(key, std::move(*value));
    }
    if (ignored > 0) {
        log::Warn("quick config {} has {} settings it can't set, ignoring them", config.name,
                  ignored);
    }
    return preset;
}

}
