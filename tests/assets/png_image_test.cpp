#include "assets/png_image.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <span>
#include <vector>

#include "assets/texture_database.hpp"
#include "image_files.hpp"
#include "temp_folder.hpp"

namespace rr::assets {
namespace {

std::vector<std::byte> PngBytes(const ImageAsset& image) {
    const test::TempFolder folder;
    test::WritePng(folder.Path() / "image.png", image);
    return test::ReadFileBytes(folder.Path() / "image.png");
}

ImageAsset TwoRows() {
    ImageAsset image = test::SolidImage(1, 2, std::byte{0xFF});
    image.rgba[4] = std::byte{0x10};
    return image;
}

void CheckDecodes(std::span<const std::byte> png, TextureFileKind kind, const ImageAsset& image) {
    const Result<ImageAsset> decoded = DecodeImage(png, kind);
    REQUIRE(decoded.has_value());
    CHECK(decoded->widthPx == image.widthPx);
    CHECK(decoded->heightPx == image.heightPx);
    CHECK(decoded->rgba == image.rgba);
    CHECK(decoded->mipLevels == FullMipLevels(image.widthPx, image.heightPx));
}

TEST_CASE("a PNG decodes whichever kind the game expects, keeping its alpha") {
    const ImageAsset image = test::SolidImage(3, 2, std::byte{0x80});
    const std::vector<std::byte> png = PngBytes(image);

    CHECK(IsPng(png));
    CheckDecodes(png, TextureFileKind::Bmp, image);
    CheckDecodes(png, TextureFileKind::Tga, image);
}

TEST_CASE("a PNG texture skips the colour key but still flips") {
    const TextureDefinition definition{.name = "BRICK",
                                       .fileKind = TextureFileKind::Tga,
                                       .flipsVertically = true,
                                       .colorKey = ColorKey{.red = 0xFF, .green = 0, .blue = 0xFF}};

    const Result<ImageAsset> decoded = DecodeTexture(PngBytes(TwoRows()), definition);

    REQUIRE(decoded.has_value());
    CHECK(decoded->rgba[0] == std::byte{0x10});
    CHECK(decoded->rgba[3] == std::byte{0xFF});
    CHECK(decoded->rgba[7] == std::byte{0xFF});
    CHECK(decoded->mipLevels == 2);
}

TEST_CASE("bytes that aren't a whole PNG fail as invalid data") {
    std::vector<std::byte> png = PngBytes(TwoRows());
    png.resize(png.size() / 2);

    CHECK_FALSE(IsPng(std::vector<std::byte>(4)));
    CHECK(DecodePng(std::vector<std::byte>(4)).error().code == ErrorCode::InvalidData);
    CHECK(DecodePng(png).error().code == ErrorCode::InvalidData);
}

}
}
