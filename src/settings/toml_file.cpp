#include "settings/toml_file.hpp"

#include <format>
#include <fstream>
#include <iterator>
#include <string>
#include <system_error>
#include <utility>

#include "core/utf8_path.hpp"

namespace rr::settings {

Result<toml::table> ReadTomlFile(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        std::error_code error;
        if (!std::filesystem::exists(path, error)) {
            return toml::table{};
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
    return std::move(parsed).table();
}

Result<void> WriteTomlFile(const toml::table& table, const std::filesystem::path& path,
                           std::string_view header) {
    std::error_code error;
    std::filesystem::create_directories(path.parent_path(), error);
    std::filesystem::path temporary = path;
    temporary += ".tmp";
    {
        std::ofstream file(temporary, std::ios::binary | std::ios::trunc);
        file << header << table << '\n';
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
