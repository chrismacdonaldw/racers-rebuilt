#include "core/utf8_path.hpp"

#include <algorithm>
#include <ranges>

namespace rr {

std::filesystem::path PathFromUtf8(std::string_view utf8) {
    std::u8string text(utf8.size(), u8'\0');
    std::ranges::transform(utf8, text.begin(), [](char c) { return static_cast<char8_t>(c); });
    return {text};
}

std::string Utf8FromPath(const std::filesystem::path& path) {
    std::u8string text = path.u8string();
    std::string utf8(text.size(), '\0');
    std::ranges::transform(text, utf8.begin(), [](char8_t c) { return static_cast<char>(c); });
    return utf8;
}

}
