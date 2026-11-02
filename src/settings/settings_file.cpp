#include "settings/settings_file.hpp"

#include <format>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "core/log.hpp"
#include "core/utf8_path.hpp"
#include "settings/toml_file.hpp"

namespace rr::settings {
namespace {

struct PendingTable {
    const toml::table* table = nullptr;
    std::string prefix;
};

std::optional<SettingValue> ValueOf(const toml::node& node) {
    if (const auto* value = node.as_boolean()) {
        return value->get();
    }
    if (const auto* value = node.as_integer()) {
        return value->get();
    }
    if (const auto* value = node.as_floating_point()) {
        return value->get();
    }
    if (const auto* value = node.as_string()) {
        return value->get();
    }
    return std::nullopt;
}

void AssignAll(Registry& registry, const toml::table& root, std::string_view fileName) {
    std::vector<PendingTable> pending{{.table = &root, .prefix = {}}};
    while (!pending.empty()) {
        const PendingTable current = std::move(pending.back());
        pending.pop_back();
        for (const auto& [name, node] : *current.table) {
            std::string key = current.prefix.empty()
                                  ? std::string(name.str())
                                  : std::format("{}.{}", current.prefix, name.str());
            if (const toml::table* child = node.as_table()) {
                pending.push_back({.table = child, .prefix = std::move(key)});
                continue;
            }
            const std::optional<SettingValue> value = ValueOf(node);
            const Result<void> assigned =
                value
                    ? registry.Assign(key, *value)
                    : Fail(ErrorCode::InvalidData, std::format("{} has an unsupported type", key));
            if (!assigned) {
                log::Warn("{}: {}, skipping it", fileName, assigned.error().message);
            }
        }
    }
}

toml::table* TableFor(toml::table& root, std::string_view& key) {
    toml::table* table = &root;
    for (std::size_t dot = key.find('.'); dot != std::string_view::npos; dot = key.find('.')) {
        const std::string_view part = key.substr(0, dot);
        if (!table->contains(part)) {
            table->insert(part, toml::table{});
        }
        table = table->get(part)->as_table();
        if (table == nullptr) {
            return nullptr;
        }
        key.remove_prefix(dot + 1);
    }
    return table;
}

toml::table ToToml(const Registry& registry) {
    toml::table root;
    for (const auto& [key, value] : registry.AllValues()) {
        std::string_view name = key;
        toml::table* table = TableFor(root, name);
        if (table == nullptr) {
            log::Warn("settings key {} clashes with a table of the same name, not saving it", key);
            continue;
        }
        std::visit([&](const auto& current) { table->insert_or_assign(name, current); }, value);
    }
    return root;
}

}

Result<void> LoadSettings(Registry& registry, const std::filesystem::path& path) {
    return ReadTomlFile(path).transform(
        [&](const toml::table& table) { AssignAll(registry, table, Utf8FromPath(path)); });
}

Result<void> SaveSettings(const Registry& registry, const std::filesystem::path& path) {
    return WriteTomlFile(ToToml(registry), path);
}

}
