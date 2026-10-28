#include "assets/bmp_image.hpp"

#include <doctest/doctest.h>

#include <cstddef>
#include <cstdint>
#include <vector>

#include "assets/token_bytes.hpp"

namespace rr::assets {
namespace {

using test::Bytes;
using test::Join;

void AppendLittleEndian(std::vector<std::byte>& bytes, std::uint32_t value, std::size_t size) {
    for (std::size_t i = 0; i < size; ++i) {
        bytes.push_back(static_cast<std::byte>(value >> (8 * i)));
    }
}

struct WindowsBmp {
    std::int32_t width = 0;
    std::int32_t height = 0;
    std::uint16_t bitsPerPixel = 0;
    std::uint32_t compression = 0;
    std::uint32_t colorsUsed = 0;
};

std::vector<std::byte> WindowsHeader(const WindowsBmp& bmp) {
    constexpr std::uint32_t kInfoBytes = 40;
    constexpr std::uint32_t kHeaderBytes = 14 + kInfoBytes;
    std::vector<std::byte> bytes = Bytes({'B', 'M'});
    AppendLittleEndian(bytes, 0, 4);
    AppendLittleEndian(bytes, 0, 4);
    AppendLittleEndian(bytes, kHeaderBytes + (bmp.colorsUsed * 4), 4);
    AppendLittleEndian(bytes, kInfoBytes, 4);
    AppendLittleEndian(bytes, static_cast<std::uint32_t>(bmp.width), 4);
    AppendLittleEndian(bytes, static_cast<std::uint32_t>(bmp.height), 4);
    AppendLittleEndian(bytes, 1, 2);
    AppendLittleEndian(bytes, bmp.bitsPerPixel, 2);
    AppendLittleEndian(bytes, bmp.compression, 4);
    AppendLittleEndian(bytes, 0, 12);
    AppendLittleEndian(bytes, bmp.colorsUsed, 4);
    AppendLittleEndian(bytes, 0, 4);
    return bytes;
}

std::vector<std::byte> Repeat(const std::vector<std::byte>& pixel, std::size_t count) {
    std::vector<std::byte> pixels;
    for (std::size_t i = 0; i < count; ++i) {
        pixels.insert(pixels.end(), pixel.begin(), pixel.end());
    }
    return pixels;
}

std::vector<std::byte> RawChunk(const std::vector<std::byte>& rows) {
    const auto size = static_cast<std::uint8_t>(rows.size());
    return Join({Bytes({size, 0x00, size, 0x00}), rows});
}

TEST_CASE("4-bit rows pack two pixels per byte, high nibble first") {
    const std::vector bmp = Join({
        Bytes({0x04, 0x02, 0x03, 0x00, 0x02, 0x00}),
        Bytes({0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x00}),
        RawChunk(Bytes({0x01, 0x20, 0x21, 0x00})),
    });

    const Result<ImageAsset> image = DecodeBmp(bmp);

    REQUIRE(image.has_value());
    CHECK(image->widthPx == 3);
    CHECK(image->heightPx == 2);
    CHECK(image->rgba == Bytes({
                             0xFF, 0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0xFF,
                             0x00, 0x00, 0xFF, 0xFF, 0x00, 0xFF, 0x00, 0xFF, 0xFF, 0x00, 0x00, 0xFF,
                         }));
}

TEST_CASE("8-bit rows may span compressed and raw chunks") {
    const std::vector bmp = Join({
        Bytes({0x08, 0x01, 0x10, 0x00, 0x02, 0x00}),
        Bytes({0x00, 0x00, 0x00, 0x30, 0x20, 0x10}),
        Bytes({0x14, 0x00, 0x07, 0x00, 0x01, 0xC0, 0x00, 0x01, 0x01, 0x00, 0x00}),
        RawChunk(std::vector<std::byte>(12)),
    });

    const Result<ImageAsset> image = DecodeBmp(bmp);

    REQUIRE(image.has_value());
    CHECK(image->rgba == Join({Repeat(Bytes({0x10, 0x20, 0x30, 0xFF}), 20),
                               Repeat(Bytes({0x00, 0x00, 0x00, 0xFF}), 12)}));
}

TEST_CASE("24-bit pixels are stored blue first and come out opaque") {
    const std::vector bmp = Join({
        Bytes({0x98, 0x00, 0x02, 0x00, 0x01, 0x00}),
        RawChunk(Bytes({0x01, 0x02, 0x03, 0x04, 0x05, 0x06})),
    });

    const Result<ImageAsset> image = DecodeBmp(bmp);

    REQUIRE(image.has_value());
    CHECK(image->rgba == Bytes({0x03, 0x02, 0x01, 0xFF, 0x06, 0x05, 0x04, 0xFF}));
}

TEST_CASE("32-bit pixels keep their alpha") {
    const std::vector bmp = Join({
        Bytes({0xA0, 0x00, 0x01, 0x00, 0x01, 0x00}),
        RawChunk(Bytes({0x01, 0x02, 0x03, 0x80})),
    });

    const Result<ImageAsset> image = DecodeBmp(bmp);

    REQUIRE(image.has_value());
    CHECK(image->rgba == Bytes({0x03, 0x02, 0x01, 0x80}));
}

TEST_CASE("Windows BMP rows are padded to four bytes and stored bottom first") {
    const std::vector bmp = Join({
        WindowsHeader({.width = 2, .height = 2, .bitsPerPixel = 24}),
        Bytes({0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x00, 0x00}),
        Bytes({0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x00, 0x00}),
    });

    const Result<ImageAsset> image = DecodeBmp(bmp);

    REQUIRE(image.has_value());
    CHECK(image->widthPx == 2);
    CHECK(image->heightPx == 2);
    CHECK(image->rgba == Bytes({
                             0x13,
                             0x12,
                             0x11,
                             0xFF,
                             0x16,
                             0x15,
                             0x14,
                             0xFF,
                             0x03,
                             0x02,
                             0x01,
                             0xFF,
                             0x06,
                             0x05,
                             0x04,
                             0xFF,
                         }));
}

TEST_CASE("a Windows BMP with a negative height is stored top first") {
    const std::vector bmp = Join({
        WindowsHeader({.width = 1, .height = -2, .bitsPerPixel = 8, .colorsUsed = 2}),
        Bytes({0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x00, 0x00}),
        Bytes({0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00}),
    });

    const Result<ImageAsset> image = DecodeBmp(bmp);

    REQUIRE(image.has_value());
    CHECK(image->rgba == Bytes({0xFF, 0x00, 0x00, 0xFF, 0x00, 0x00, 0xFF, 0xFF}));
}

TEST_CASE("malformed BMPs fail") {
    SUBCASE("a compressed Windows BMP") {
        CHECK_FALSE(DecodeBmp(Join({WindowsHeader({.width = 1,
                                                   .height = 1,
                                                   .bitsPerPixel = 8,
                                                   .compression = 1,
                                                   .colorsUsed = 1}),
                                    Bytes({0, 0, 0, 0, 0, 0, 0, 0})}))
                        .has_value());
    }
    SUBCASE("a Windows BMP wider than 65,535 pixels") {
        CHECK_FALSE(
            DecodeBmp(Join({WindowsHeader({.width = 0x08000001, .height = 1, .bitsPerPixel = 32}),
                            Bytes({0x00, 0x00, 0x00, 0x00})}))
                .has_value());
    }
    SUBCASE("a Windows BMP cut short") {
        CHECK_FALSE(DecodeBmp(Join({WindowsHeader({.width = 2, .height = 2, .bitsPerPixel = 24}),
                                    Bytes({0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x00, 0x00})}))
                        .has_value());
    }
    SUBCASE("an unsupported depth") {
        CHECK_FALSE(
            DecodeBmp(Join({Bytes({0x90, 0x00, 0x01, 0x00, 0x01, 0x00}), RawChunk(Bytes({0, 0}))}))
                .has_value());
    }
    SUBCASE("a palette index past the palette") {
        CHECK_FALSE(DecodeBmp(Join({Bytes({0x08, 0x00, 0x01, 0x00, 0x01, 0x00}),
                                    Bytes({0x00, 0x00, 0x00}), RawChunk(Bytes({0x01}))}))
                        .has_value());
    }
    SUBCASE("a chunk past the image") {
        CHECK_FALSE(DecodeBmp(Join({Bytes({0x08, 0x00, 0x01, 0x00, 0x01, 0x00}),
                                    Bytes({0x00, 0x00, 0x00}), RawChunk(Bytes({0x00, 0x00}))}))
                        .has_value());
    }
    SUBCASE("missing rows") {
        CHECK_FALSE(DecodeBmp(Join({Bytes({0x08, 0x00, 0x01, 0x00, 0x02, 0x00}),
                                    Bytes({0x00, 0x00, 0x00}), RawChunk(Bytes({0x00}))}))
                        .has_value());
    }
    SUBCASE("no pixels") {
        CHECK_FALSE(DecodeBmp(Bytes({0x98, 0x00, 0x00, 0x00, 0x01, 0x00})).has_value());
    }
}

}
}
