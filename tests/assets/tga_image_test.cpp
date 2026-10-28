#include "assets/tga_image.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <vector>

#include "assets/token_bytes.hpp"

namespace rr::assets {
namespace {

using test::Bytes;
using test::Join;

std::vector<std::byte> Header(std::uint8_t imageType, std::uint8_t widthPx, std::uint8_t heightPx,
                              std::uint8_t depth, std::uint8_t descriptor) {
    return Bytes({0x00, 0x00, imageType, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                  widthPx, 0x00, heightPx, 0x00, depth, descriptor});
}

TEST_CASE("32-bit rows stored bottom first come out top first") {
    const std::vector tga = Join({
        Header(2, 1, 2, 32, 0x08),
        Bytes({0x01, 0x02, 0x03, 0x04}),
        Bytes({0x05, 0x06, 0x07, 0x08}),
    });

    const Result<ImageAsset> image = DecodeTga(tga);

    REQUIRE(image.has_value());
    CHECK(image->widthPx == 1);
    CHECK(image->heightPx == 2);
    CHECK(image->rgba == Bytes({0x07, 0x06, 0x05, 0x08, 0x03, 0x02, 0x01, 0x04}));
}

TEST_CASE("24-bit rows with a top origin keep their order and come out opaque") {
    const std::vector tga = Join({
        Header(2, 2, 1, 24, 0x20),
        Bytes({0x01, 0x02, 0x03, 0x04, 0x05, 0x06}),
    });

    const Result<ImageAsset> image = DecodeTga(tga);

    REQUIRE(image.has_value());
    CHECK(image->rgba == Bytes({0x03, 0x02, 0x01, 0xFF, 0x06, 0x05, 0x04, 0xFF}));
}

TEST_CASE("the image id is skipped") {
    std::vector tga = Join({Header(2, 1, 1, 24, 0x20), Bytes({0x0A, 0x0B, 0x01, 0x02, 0x03})});
    tga[0] = std::byte{2};

    const Result<ImageAsset> image = DecodeTga(tga);

    REQUIRE(image.has_value());
    CHECK(image->rgba == Bytes({0x03, 0x02, 0x01, 0xFF}));
}

TEST_CASE("unsupported or short TGAs fail") {
    SUBCASE("run-length encoded") {
        CHECK_FALSE(DecodeTga(Join({Header(10, 1, 1, 24, 0), Bytes({0x00, 0x01, 0x02, 0x03})}))
                        .has_value());
    }
    SUBCASE("16-bit") {
        CHECK_FALSE(DecodeTga(Join({Header(2, 1, 1, 16, 0), Bytes({0x00, 0x00})})).has_value());
    }
    SUBCASE("pixels cut short") {
        CHECK_FALSE(
            DecodeTga(Join({Header(2, 2, 1, 24, 0), Bytes({0x00, 0x00, 0x00})})).has_value());
    }
    SUBCASE("header cut short") {
        CHECK_FALSE(DecodeTga(Bytes({0x00, 0x00, 0x02})).has_value());
    }
}

}
}
