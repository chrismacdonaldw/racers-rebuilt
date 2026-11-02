#include "settings/toml_file.hpp"

#include <algorithm>
#include <format>
#include <fstream>
#include <iterator>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>

#include "core/utf8_path.hpp"

namespace rr::settings {
namespace {

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
    const std::string fileName = Utf8FromPath(path);
    toml::parse_result parsed = toml::parse(text, fileName);
    if (!parsed) {
        return Fail(ErrorCode::InvalidData,
                    std::format("{} line {}: {}", fileName, parsed.error().source().begin.line,
                                parsed.error().description()));
    }
    return TomlFile{.table = std::move(parsed).table(), .leadingComments = LeadingComments(text)};
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
