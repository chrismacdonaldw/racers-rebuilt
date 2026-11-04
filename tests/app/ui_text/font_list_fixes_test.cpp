#include "app/ui_text/font_list_fixes.hpp"

#include <doctest/doctest.h>

#include <string_view>
#include <vector>

namespace rr::app {
namespace {

constexpr std::u16string_view kRetailEnglish =
    u"ETAONRISHDLFCMUGYPWBVKXJQZ0123456789()&"
    u"\x05\x06\x07"
    u"-";

assets::FontDefinition Font(std::u16string_view characters) {
    return {.name = "fontmenu", .characters = {characters.begin(), characters.end()}};
}

TEST_CASE("the English menu font gets the entry its list skips after the brackets") {
    const std::optional<std::vector<char16_t>> fixed = FixedCharacters(Font(kRetailEnglish));

    REQUIRE(fixed.has_value());
    const std::vector<char16_t> characters = fixed.value_or(std::vector<char16_t>{});
    REQUIRE(characters.size() == kRetailEnglish.size() + 1);
    CHECK(characters[37] == u')');
    CHECK(characters[38] == char16_t{0x92});
    CHECK(characters[39] == u'&');
    CHECK(characters.back() == u'-');
}

TEST_CASE("other font lists are left as they are") {
    CHECK_FALSE(FixedCharacters(Font(u"ABC")).has_value());
    CHECK_FALSE(FixedCharacters(Font(kRetailEnglish.substr(1))).has_value());
}

}
}
