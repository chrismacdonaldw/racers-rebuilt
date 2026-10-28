#include "assets/texture_database.hpp"

#include <doctest/doctest.h>

#include <vector>

#include "assets/token_bytes.hpp"

namespace rr::assets {
namespace {

using test::Bytes;
using test::Join;

TEST_CASE("texture definitions read every flag") {
    const std::vector tdb = Join({
        Bytes({0x27, 0x07, 0x0B, 0x02, 0x08, 0x05}),
        Bytes({0x27, 0x02, 'w', 'a', 't', 'e', 'r', 0x00, 0x05}),
        Bytes({0x2A, 0x28, 0x2C, 0x0B, 0x01, 0x0B, 0x02, 0x0B, 0xFF, 0x2D, 0x06}),
        Bytes({0x27, 0x02, 's', 'm', 'o', 'k', 'e', 0x00, 0x05}),
        Bytes({0x2B, 0x29, 0x0B, 0x04, 0x2E, 0x06}),
        Bytes({0x06}),
    });

    const Result<std::vector<TextureDefinition>> textures = ParseTextureDatabase(tdb);

    REQUIRE(textures.has_value());
    REQUIRE(textures->size() == 2);
    const TextureDefinition& water = textures->at(0);
    CHECK(water.name == "water");
    CHECK(water.fileKind == TextureFileKind::Bmp);
    CHECK(water.FileName() == "water.BMP");
    CHECK(water.flipsVertically);
    CHECK(water.colorKey == ColorKey{.red = 1, .green = 2, .blue = 255});
    CHECK(water.hasFlag2D);
    CHECK_FALSE(water.hasFlag2E);
    CHECK(water.mipmapCount == 0);

    const TextureDefinition& smoke = textures->at(1);
    CHECK(smoke.FileName() == "smoke.TGA");
    CHECK_FALSE(smoke.flipsVertically);
    CHECK_FALSE(smoke.colorKey.has_value());
    CHECK(smoke.mipmapCount == 4);
    CHECK(smoke.hasFlag2E);
}

Result<std::vector<TextureDefinition>> ParseOneTexture(const std::vector<std::byte>& fields) {
    return ParseTextureDatabase(Join({
        Bytes({0x27, 0x07, 0x0B, 0x01, 0x08, 0x05}),
        Bytes({0x27, 0x02, 'a', 0x00, 0x05}),
        fields,
        Bytes({0x06, 0x06}),
    }));
}

TEST_CASE("an unknown texture field fails") {
    CHECK(ParseOneTexture(Bytes({0x2A})).has_value());
    CHECK_FALSE(ParseOneTexture(Bytes({0x30})).has_value());
}

TEST_CASE("color key channels must fit in a byte") {
    CHECK_FALSE(
        ParseOneTexture(Bytes({0x2C, 0x0B, 0x00, 0x0B, 0x00, 0x0E, 0x00, 0x01})).has_value());
    CHECK_FALSE(
        ParseOneTexture(Bytes({0x2C, 0x0B, 0x00, 0x0B, 0x00, 0x0D, 0xFF, 0xFF})).has_value());
}

TEST_CASE("data after the texture list fails") {
    CHECK_FALSE(
        ParseTextureDatabase(Bytes({0x27, 0x07, 0x0B, 0x00, 0x08, 0x05, 0x06, 0x06})).has_value());
}

TEST_CASE("a color key makes matching pixels transparent") {
    ImageAsset image{.widthPx = 2,
                     .heightPx = 1,
                     .rgba = Bytes({0x01, 0x02, 0x03, 0xFF, 0x01, 0x02, 0x04, 0xFF})};

    ApplyColorKey(image, {.red = 1, .green = 2, .blue = 3});

    CHECK(image.rgba == Bytes({0x01, 0x02, 0x03, 0x00, 0x01, 0x02, 0x04, 0xFF}));
}

}
}
