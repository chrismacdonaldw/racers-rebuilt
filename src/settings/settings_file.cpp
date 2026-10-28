#include "settings/settings_file.hpp"

#include <format>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#include "core/log.hpp"
#include "core/utf8_path.hpp"

#include <toml++/toml.hpp>

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
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::error_code error;
        if (!std::filesystem::exists(path, error)) {
            return {};
        }
        return Fail(ErrorCode::Io, std::format("can't read {}", Utf8FromPath(path)));
    }
    const std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    const std::string fileName = Utf8FromPath(path);

    toml::parse_result parsed = toml::parse(text, fileName);
    if (!parsed) {
        return Fail(ErrorCode::InvalidData,
                    std::format("{} line {}: {}", fileName, parsed.error().source().begin.line,
                                parsed.error().description()));
    }
    AssignAll(registry, parsed.table(), fileName);
    return {};
}

Result<void> SaveSettings(const Registry& registry, const std::filesystem::path& path) {
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    std::filesystem::path temporary = path;
    temporary += ".tmp";
    {
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        file << ToToml(registry) << '\n';
        if (!file) {
            return Fail(ErrorCode::Io, std::format("can't write {}", Utf8FromPath(temporary)));
        }
    }
    std::filesystem::rename(temporary, path, error);
    if (error) {
        return Fail(ErrorCode::Io,
                    std::format("can't replace {}: {}", Utf8FromPath(path), error.message()));
    }
    return {};
}

}
