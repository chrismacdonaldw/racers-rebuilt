#include "app/texture_keys.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "assets/token_bytes.hpp"
#include "game_layers.hpp"

namespace rr::app {
namespace {

using test::Bytes;
using test::Ints;
using test::Join;

constexpr std::uint8_t kEntry = 0x27;
constexpr std::uint8_t kTextureKey = 0x2C;
constexpr std::uint8_t kImageKey = 0x2B;
constexpr std::uint8_t kFontKey = 0x2A;

std::vector<std::byte> Table(std::uint8_t keyField, std::string_view name, std::uint8_t red) {
    return test::NamedLibrary(
        kEntry, {test::NamedEntry(kEntry, name, Join({Bytes({keyField}), Ints({red, 0, 0})}))});
}

assets::ColorKey Red(std::uint8_t red) {
    return {.red = red};
}

ColorKeys Collect(const test::GameLayers& layers) {
    const std::unique_ptr<assets::Vfs> vfs = layers.Mount();
    Result<ColorKeys> keys = CollectColorKeys(*vfs, test::GameLayers::kGameLayer);
    REQUIRE(keys.has_value());
    return std::move(*keys);
}

}

TEST_CASE("colour keys come from texture, image and font tables by file name") {
    const test::GameLayers layers;
    layers.WriteGame("GAMEDATA/COMMON/COMMON.TDB", Table(kTextureKey, "brick", 1));
    layers.WriteGame("MENUDATA/MENU.IDB", Table(kImageKey, "logo", 2));
    layers.WriteGame("MENUDATA/FONTS.FDB", Table(kFontKey, "fontmenu", 3));
    layers.WriteGame("MENUDATA/PLAIN.IDB",
                     test::NamedLibrary(kEntry, {test::NamedEntry(kEntry, "plain", {})}));

    const ColorKeys keys = Collect(layers);

    CHECK(keys.byName.at("BRICK.TGA") == Red(1));
    CHECK(keys.byName.at("LOGO.TGA") == Red(2));
    CHECK(keys.byName.at("FONTMENU.TGA") == Red(3));
    CHECK_FALSE(keys.byName.contains("PLAIN.TGA"));
    CHECK(keys.conflicts.empty());
}

TEST_CASE("a second key for an image keeps the first, and a broken table is skipped") {
    const test::GameLayers layers;
    layers.WriteGame("GAMEDATA/A/A.TDB", Table(kTextureKey, "brick", 1));
    layers.WriteGame("GAMEDATA/B/B.TDB", Table(kTextureKey, "brick", 2));
    layers.WriteGame("GAMEDATA/C/C.TDB", Table(kTextureKey, "brick", 3));
    layers.WriteGame("GAMEDATA/D/D.TDB", Bytes({0x99}));

    const ColorKeys keys = Collect(layers);

    CHECK(keys.byName.at("BRICK.TGA") == Red(1));
    CHECK(keys.conflicts == std::vector<std::string>{"BRICK.TGA"});
}

}
