#include "app/ui_text/font_list_fixes.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <string_view>

namespace rr::app {
namespace {

constexpr std::u16string_view kEnglishMenuFont =
    u"ETAONRISHDLFCMUGYPWBVKXJQZ0123456789()&"
    u"\x05\x06\x07"
    u"-";
constexpr std::ptrdiff_t kUnlistedCell = 38;
constexpr char16_t kFrenchQuote = 0x92;

}

std::optional<std::vector<char16_t>> FixedCharacters(const assets::FontDefinition& definition) {
    if (!std::ranges::equal(definition.characters, kEnglishMenuFont)) {
        return std::nullopt;
    }
    std::vector<char16_t> fixed = definition.characters;
    fixed.insert(std::next(fixed.begin(), kUnlistedCell), kFrenchQuote);
    return fixed;
}

}
