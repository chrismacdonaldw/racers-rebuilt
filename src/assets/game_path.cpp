#include "assets/game_path.hpp"

#include <algorithm>
#include <cstddef>
#include <format>

namespace rr::assets {
namespace {

bool IsSeparator(char c) {
    return c == '/' || c == '\\';
}

constexpr std::string_view kForbiddenCharacters = ":*?\"<>|";

bool IsNameCharacter(char c) {
    return c >= ' ' && c <= '~' && !IsSeparator(c) && !kForbiddenCharacters.contains(c);
}

char UpperChar(char c) {
    return c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : c;
}

}

std::string ToUpperAscii(std::string_view text) {
    std::string upper(text.size(), '\0');
    std::ranges::transform(text, upper.begin(), UpperChar);
    return upper;
}

Result<std::string> NormalizeGameName(std::string_view name) {
    if (name.empty() || name == "." || name == "..") {
        return Fail(ErrorCode::InvalidData, std::format("\"{}\" isn't a valid file name", name));
    }
    if (!std::ranges::all_of(name, IsNameCharacter)) {
        return Fail(ErrorCode::InvalidData,
                    std::format("\"{}\" has characters that aren't allowed in file names", name));
    }
    return ToUpperAscii(name);
}

Result<std::string> NormalizeGamePath(std::string_view path) {
    while (!path.empty() && IsSeparator(path.front())) {
        path.remove_prefix(1);
    }
    while (!path.empty() && IsSeparator(path.back())) {
        path.remove_suffix(1);
    }

    std::string normalized;
    std::string_view rest = path;
    while (!rest.empty()) {
        const std::size_t end = std::min(rest.find_first_of("/\\"), rest.size());
        Result<std::string> name = NormalizeGameName(rest.substr(0, end));
        if (!name) {
            return Fail(ErrorCode::InvalidData, std::format("\"{}\" isn't a valid game path: {}",
                                                            path, name.error().message));
        }
        normalized = JoinGamePath(normalized, *name);
        rest.remove_prefix(std::min(end + 1, rest.size()));
    }
    return normalized;
}

std::string JoinGamePath(std::string_view folder, std::string_view name) {
    if (folder.empty()) {
        return std::string(name);
    }
    return std::format("{}/{}", folder, name);
}

}
