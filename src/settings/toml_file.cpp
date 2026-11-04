#include "settings/toml_file.hpp"

#include <algorithm>
#include <format>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <variant>
#include <vector>

#include "core/log.hpp"
#include "core/utf8_path.hpp"

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

std::string LeadingComments(std::string_view text) {
    std::size_t end = 0;
    bool hasComment = false;
    while (end < text.size()) {
        const std::size_t next = std::min(text.find('\n', end), text.size() - 1) + 1;
        const std::string_view line = text.substr(end, next - end);
        const std::size_t first = line.find_first_not_of(" \t\r\n");
        if (first != std::string_view::npos && line[first] != '#') {
            break;
        }
        hasComment = hasComment || first != std::string_view::npos;
        end = next;
    }
    return hasComment ? std::string(text.substr(0, end)) : std::string();
}

}

Result<toml::table> ParseToml(std::string_view text, std::string_view sourceName) {
    toml::parse_result parsed = toml::parse(text, sourceName);
    if (!parsed) {
        return Fail(ErrorCode::InvalidData,
                    std::format("{} line {}: {}", sourceName, parsed.error().source().begin.line,
                                parsed.error().description()));
    }
    return std::move(parsed).table();
}

std::vector<TomlSetting> FlattenSettings(const toml::table& root) {
    std::vector<TomlSetting> settings;
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
            settings.push_back({.key = std::move(key), .value = ValueOf(node)});
        }
    }
    return settings;
}

toml::table NestSettings(const Registry::Values& values) {
    toml::table root;
    for (const auto& [key, value] : values) {
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

Result<TomlFile> ReadTomlFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::error_code error;
        if (!std::filesystem::exists(path, error)) {
            return TomlFile{};
        }
        return Fail(ErrorCode::Io, std::format("can't read {}", Utf8FromPath(path)));
    }
    const std::string text{std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>()};
    return ParseToml(text, Utf8FromPath(path)).transform([&](toml::table table) {
        return TomlFile{.table = std::move(table), .leadingComments = LeadingComments(text)};
    });
}

Result<void> WriteTomlFile(const TomlFile& file, const std::filesystem::path& path) {
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    std::filesystem::path temporary = path;
    temporary += ".tmp";
    {
        std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
        out << file.leadingComments << file.table << '\n';
        if (!out) {
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

Result<void> ReplaceTomlKeys(const std::filesystem::path& path, toml::table keys,
                             std::span<const std::string_view> owned, std::string_view header) {
    Result<TomlFile> stored = ReadTomlFile(path);
    if (!stored) {
        return std::unexpected(std::move(stored.error()));
    }
    for (const std::string_view key : owned) {
        stored->table.erase(key);
    }
    for (auto&& [key, value] : keys) {
        stored->table.insert_or_assign(key, std::move(value));
    }
    if (stored->leadingComments.empty()) {
        stored->leadingComments = header;
    }
    return WriteTomlFile(*stored, path);
}

}
