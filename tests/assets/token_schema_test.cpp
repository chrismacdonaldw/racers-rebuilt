#include "assets/token_schema.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "assets/token_bytes.hpp"

namespace rr::assets {
namespace {

using test::Bytes;

struct Surface {
    std::string name;
    std::array<std::int32_t, 4> color{};
    std::string texture;
    float shine = 0.0F;
    bool isTwoSided = false;
    std::vector<std::int32_t> layers;
};

constexpr std::array kSurfaceFields{
    Field<&Surface::color>(0x28),      Field<&Surface::texture>(0x29), Field<&Surface::shine>(0x2A),
    Field<&Surface::isTwoSided>(0x2B), Field<&Surface::layers>(0x2C),
};

struct Library {
    std::vector<Surface> surfaces;
};

Result<void> ReadSurfaces(TokenReader& reader, Library& library) {
    return ReadList(reader, [&] {
        Surface& surface = library.surfaces.emplace_back();
        return reader.Expect(TokenKind::Field)
            .and_then([&](const Token&) { return reader.ReadString(); })
            .and_then([&](std::string_view name) {
                surface.name = name;
                return ReadBlock<Surface>(reader, kSurfaceFields, surface);
            });
    });
}

constexpr std::array kLibraryFields{FieldRule<Library>{.id = 0x27, .read = ReadSurfaces}};

TEST_CASE("declared fields fill a nested structure") {
    const std::vector bytes = test::Join({
        Bytes({0x27, 0x07, 0x0B, 0x02, 0x08, 0x05}),
        Bytes({0x27, 0x02, 'r', 'e', 'd', 0x00, 0x05}),
        Bytes({0x28, 0x0B, 0xFF, 0x0B, 0x00, 0x0B, 0x00, 0x0B, 0x7F}),
        Bytes({0x2B}),
        Bytes({0x2C, 0x07, 0x0B, 0x02, 0x08, 0x05, 0x0B, 0x03, 0x0B, 0x04, 0x06}),
        Bytes({0x06}),
        Bytes({0x27, 0x02, 'b', 'l', 'u', 'e', 0x00, 0x05}),
        Bytes({0x29, 0x02, 'b', 'l', 'u', 'e', 0x00}),
        Bytes({0x2A, 0x10, 0x10, 0x00}),
        Bytes({0x06, 0x06}),
    });
    TokenReader reader(bytes);
    Library library;

    REQUIRE(ReadFields<Library>(reader, kLibraryFields, library).has_value());

    CHECK(reader.Next()->kind == TokenKind::End);
    REQUIRE(library.surfaces.size() == 2);
    const Surface& red = library.surfaces[0];
    CHECK(red.name == "red");
    CHECK(red.color == std::array<std::int32_t, 4>{255, 0, 0, 127});
    CHECK(red.isTwoSided);
    CHECK(red.layers == std::vector<std::int32_t>{3, 4});
    const Surface& blue = library.surfaces[1];
    CHECK(blue.texture == "blue");
    CHECK(blue.shine == 0.5F);
    CHECK_FALSE(blue.isTwoSided);
}

TEST_CASE("fields stop at a closing brace and leave it unread") {
    const std::vector bytes = Bytes({0x2A, 0x10, 0x20, 0x00, 0x06});
    TokenReader reader(bytes);
    Surface surface;

    REQUIRE(ReadFields<Surface>(reader, kSurfaceFields, surface).has_value());

    CHECK(surface.shine == 1.0F);
    CHECK(reader.Next()->kind == TokenKind::CloseBrace);
}

TEST_CASE("an unknown field fails with its id and offset") {
    const std::vector bytes = Bytes({0x2B, 0x13, 0x00, 0x01});
    TokenReader reader(bytes);
    Surface surface;

    const Result<void> read = ReadFields<Surface>(reader, kSurfaceFields, surface);

    REQUIRE_FALSE(read.has_value());
    CHECK(read.error().message == "unknown field 0x100 at offset 1");
}

TEST_CASE("a value of the wrong type fails") {
    const std::vector bytes = Bytes({0x2A, 0x0B, 0x01});
    TokenReader reader(bytes);
    Surface surface;

    CHECK(ReadFields<Surface>(reader, kSurfaceFields, surface).error().code ==
          ErrorCode::InvalidData);
}

TEST_CASE("unsigned values fail outside their range") {
    const std::vector bytes = Bytes({0x0E, 0xFF, 0xFF, 0x0D, 0xFF, 0xFF, 0x0E, 0x00, 0x01});
    TokenReader reader(bytes);
    std::uint16_t wide = 0;
    std::uint16_t negative = 0;
    std::uint8_t narrow = 0;

    CHECK(ReadValue(reader, wide).has_value());
    CHECK(wide == 0xFFFF);
    CHECK_FALSE(ReadValue(reader, negative).has_value());
    CHECK_FALSE(ReadValue(reader, narrow).has_value());
}

TEST_CASE("a block without its closing brace fails") {
    const std::vector bytes = Bytes({0x05, 0x2B});
    TokenReader reader(bytes);
    Surface surface;

    CHECK(ReadBlock<Surface>(reader, kSurfaceFields, surface).error().message ==
          "expected '}' at offset 2, found end of data");
}

TEST_CASE("a list shorter than its count fails") {
    const std::vector bytes = Bytes({0x07, 0x0B, 0x03, 0x08, 0x05, 0x0B, 0x01, 0x06});
    TokenReader reader(bytes);
    std::vector<std::int32_t> values;

    CHECK(ReadValue(reader, values).error().code == ErrorCode::InvalidData);
}

namespace {

Result<void> ReadInts(TokenReader& reader, std::vector<std::int32_t>& values) {
    return ReadEntryList(reader, [&] { return ReadValue(reader, values.emplace_back()); });
}

}

TEST_CASE("an entry list reads its elements like a list") {
    const std::vector bytes = Bytes({0x07, 0x0B, 0x02, 0x08, 0x05, 0x0B, 0x04, 0x0B, 0x05, 0x06});
    TokenReader reader(bytes);
    std::vector<std::int32_t> values;

    REQUIRE(ReadInts(reader, values).has_value());
    CHECK(values == std::vector<std::int32_t>{4, 5});
}

TEST_CASE("an empty entry list may end at its count") {
    const std::vector bare = Bytes({0x07, 0x0B, 0x00, 0x08});
    const std::vector braced = Bytes({0x07, 0x0B, 0x00, 0x08, 0x05, 0x06});
    const std::vector unbraced = Bytes({0x07, 0x0B, 0x00, 0x08, 0x0B, 0x01});
    std::vector<std::int32_t> values;

    TokenReader bareReader(bare);
    TokenReader bracedReader(braced);
    TokenReader unbracedReader(unbraced);

    CHECK(ReadInts(bareReader, values).has_value());
    CHECK(ReadInts(bracedReader, values).has_value());
    CHECK_FALSE(ReadInts(unbracedReader, values).has_value());
    CHECK(values.empty());
}

}
}
