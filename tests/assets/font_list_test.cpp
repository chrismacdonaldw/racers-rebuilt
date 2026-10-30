#include "assets/font_list.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "archive_survey.hpp"
#include "assets/bmp_image.hpp"
#include "assets/game_path.hpp"
#include "token_bytes.hpp"

namespace rr::assets {
namespace {

using test::Bytes;
using test::Ints;
using test::Join;
using test::Text;

std::vector<std::byte> FontList(const std::vector<std::byte>& fields) {
    return Join({Bytes({0x27}), test::ListHeader(1), Bytes({0x27}), Text("fontmenu"), Bytes({0x05}),
                 fields, Bytes({0x06, 0x06})});
}

TEST_CASE("a font reads its image, key, characters, spacing and colour") {
    const Result<std::vector<FontDefinition>> fonts = ParseFontList(FontList(Join({
        Bytes({0x28, 0x2A}),
        Ints({0, 0, 0}),
        Bytes({0x2C}),
        Ints({2}),
        Bytes({0x2B, 0x07}),
        Text("A"),
        Ints({66}),
        Bytes({0x08, 0x2D}),
        Ints({1, 2, 3}),
    })));

    REQUIRE(fonts.has_value());
    REQUIRE(fonts->size() == 1);
    const FontDefinition& font = fonts->front();
    CHECK(font.FileName() == "fontmenu.BMP");
    CHECK(font.colorKey == ColorKey{});
    CHECK(font.characters == std::vector<char16_t>{u'A', u'B'});
    CHECK(font.spacingPx == 2);
    CHECK(font.textColor == std::array<std::uint8_t, 3>{1, 2, 3});
}

TEST_CASE("a font defaults to a white TGA") {
    const Result<std::vector<FontDefinition>> fonts = ParseFontList(FontList({}));

    REQUIRE(fonts.has_value());
    const FontDefinition& font = fonts->front();
    CHECK(font.FileName() == "fontmenu.TGA");
    CHECK_FALSE(font.colorKey.has_value());
    CHECK(font.textColor == std::array<std::uint8_t, 3>{0xFF, 0xFF, 0xFF});
}

TEST_CASE("a font character must be one character, and a font holds at most 100") {
    CHECK_FALSE(ParseFontList(FontList(Join({Bytes({0x2B, 0x07}), Text("AB"), Bytes({0x08})})))
                    .has_value());

    std::vector<std::byte> characters = Bytes({0x2B, 0x07});
    for (int i = 0; i < 101; ++i) {
        characters.insert(characters.end(), {std::byte{0x0B}, std::byte{'A'}});
    }
    characters.push_back(std::byte{0x08});
    CHECK_FALSE(ParseFontList(FontList(characters)).has_value());
}

constexpr std::array<std::byte, 4> kRed{std::byte{0xFF}, std::byte{0}, std::byte{0},
                                        std::byte{0xFF}};
constexpr std::array<std::byte, 4> kBlue{std::byte{0}, std::byte{0}, std::byte{0xFF},
                                         std::byte{0xFF}};
constexpr std::array<std::byte, 4> kInk{std::byte{0xFF}, std::byte{0xFF}, std::byte{0xFF},
                                        std::byte{0xFF}};

ImageAsset GlyphStrip(std::string_view top, std::string_view bottom) {
    ImageAsset image{.widthPx = static_cast<std::uint32_t>(top.size()), .heightPx = 2};
    for (const auto& [row, separator] : {std::pair{top, kRed}, std::pair{bottom, kBlue}}) {
        for (const char column : row) {
            const std::array<std::byte, 4>& pixel = column == 's' ? separator : kInk;
            image.rgba.insert(image.rgba.end(), pixel.begin(), pixel.end());
        }
    }
    return image;
}

TEST_CASE("glyphs are found between separator columns and sorted by character") {
    const ImageAsset image = GlyphStrip("ssxggsgssg", "ssxgssgssg");

    const FontGlyphs font = ScanGlyphs(image, std::u16string_view(u"BACD"));

    CHECK(font.spaceWidthPx == 2);
    REQUIRE(font.glyphs.size() == 3);
    CHECK(font.glyphs[0].character == u'A');
    CHECK(font.glyphs[0].leftPx == 6);
    CHECK(font.glyphs[0].widthPx == 1);
    CHECK(font.glyphs[1].character == u'B');
    CHECK(font.glyphs[1].leftPx == 3);
    CHECK(font.glyphs[1].widthPx == 2);
    CHECK(font.glyphs[2].character == u'C');
    CHECK(font.glyphs[2].leftPx == 9);
}

std::size_t FindRetailGlyphs(const JamArchive& archive, const IndexEntry& fontList,
                             const FontDefinition& font) {
    const Result<ImageAsset> image =
        test::ReadRetailFile(archive,
                             JoinGamePath(fontList.Parent(), ToUpperAscii(font.FileName())))
            .and_then([](const FileData& data) { return DecodeBmp(data.Bytes()); });
    REQUIRE(image.has_value());
    return ScanGlyphs(*image, font.characters).glyphs.size();
}

std::size_t ExpectedGlyphs(const IndexEntry& fontList, const FontDefinition& font) {
    const bool isDanishMenuFont =
        fontList.path == "MENUDATA/DANISH/GFONTS.FDB" && font.name == "fontmenu";
    return font.characters.size() - (isDanishMenuFont ? 1 : 0);
}

struct FontCounts {
    std::size_t fonts = 0;
    std::size_t glyphs = 0;
};

FontCounts CheckRetailFontList(const JamArchive& archive, const IndexEntry& fontList) {
    CAPTURE(fontList.path);
    const Result<std::vector<FontDefinition>> fonts =
        test::ParseEntry(archive, fontList, ParseFontList);
    REQUIRE(fonts.has_value());
    FontCounts counts{.fonts = fonts->size()};
    for (const FontDefinition& font : *fonts) {
        const std::size_t found = FindRetailGlyphs(archive, fontList, font);
        CHECK_MESSAGE(found == ExpectedGlyphs(fontList, font), font.name);
        counts.glyphs += found;
    }
    return counts;
}

TEST_CASE("every font in the retail game parses and finds its glyphs") {
    const std::optional<JamArchive> archive = test::OpenRetailJam();
    if (!archive) {
        return;
    }

    const std::vector<IndexEntry> fontLists = test::FilesWithExtension(*archive, "FDB");
    std::size_t fontCount = 0;
    std::size_t glyphCount = 0;
    for (const IndexEntry& fontList : fontLists) {
        const FontCounts counts = CheckRetailFontList(*archive, fontList);
        fontCount += counts.fonts;
        glyphCount += counts.glyphs;
    }
    CHECK(fontLists.size() == 20);
    CHECK(fontCount == 60);
    CHECK(glyphCount == 2941);
}

}
}
