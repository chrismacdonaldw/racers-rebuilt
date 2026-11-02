#include "assets/piece_library.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <string_view>
#include <vector>

#include "assets/token_bytes.hpp"

namespace rr::assets {
namespace {

using test::Bytes;
using test::Join;
using test::Text;

std::vector<std::byte> Short(std::int32_t value) {
    const auto bits = static_cast<std::uint16_t>(value);
    return Bytes(
        {0x0D, static_cast<std::uint8_t>(bits & 0xFFU), static_cast<std::uint8_t>(bits >> 8U)});
}

std::vector<std::byte> Stream(std::uint8_t field, std::size_t elements,
                              std::initializer_list<std::int32_t> values) {
    std::vector<std::byte> bytes =
        Join({Bytes({field}), test::ListHeader(static_cast<std::uint8_t>(elements))});
    for (const std::int32_t value : values) {
        const std::vector<std::byte> encoded = Short(value);
        bytes.insert(bytes.end(), encoded.begin(), encoded.end());
    }
    bytes.push_back(std::byte{0x06});
    return bytes;
}

struct StoredPiece {
    std::string_view name;
    std::uint16_t type = 0;
    std::uint8_t shape = 0;
    std::uint8_t commands = 0;
    std::uint8_t offset = 0;
};

std::vector<std::byte> Pieces(std::initializer_list<StoredPiece> pieces) {
    std::vector<std::byte> bytes =
        Join({Bytes({0x27}), test::ListHeader(static_cast<std::uint8_t>(pieces.size()))});
    for (const StoredPiece& piece : pieces) {
        const std::vector<std::byte> record =
            Join({Text(piece.name), Short(piece.type),
                  test::Ints({piece.shape, piece.commands, piece.offset})});
        bytes.insert(bytes.end(), record.begin(), record.end());
    }
    bytes.push_back(std::byte{0x06});
    return bytes;
}

constexpr std::array<std::array<std::uint8_t, 2>, 15> kCells{{
    {1, 1},
    {0x81, 0x80},
    {2, 3},
    {0x80, 0},
    {0x81, 0},
    {0x82, 0},
    {0x8A, 0},
    {0x8B, 0},
    {0x8C, 0},
    {1, 1},
    {0x83, 0xC1},
    {1, 0},
    {1, 1},
    {0x83, 0xC1},
    {0xFF, 0xFF},
}};

std::vector<std::byte> CellStream(std::size_t cellCount) {
    std::vector<std::byte> bytes =
        Join({Bytes({0x2C}), test::ListHeader(static_cast<std::uint8_t>(cellCount))});
    for (std::size_t i = 0; i < cellCount; ++i) {
        const std::vector<std::byte> encoded = test::Ints({kCells.at(i)[0], kCells.at(i)[1]});
        bytes.insert(bytes.end(), encoded.begin(), encoded.end());
    }
    bytes.push_back(std::byte{0x06});
    return bytes;
}

std::vector<std::byte> Library(std::initializer_list<StoredPiece> pieces,
                               std::size_t cellCount = kCells.size()) {
    return Join({Stream(0x28, 3, {0x4000, 1, 2}), Stream(0x29, 2, {256, 512, -128, 0, 0, 3840}),
                 Stream(0x2A, 1, {0, -127, 127}), Stream(0x2B, 1, {1024, -512}),
                 CellStream(cellCount), Pieces(pieces)});
}

const std::initializer_list<StoredPiece> kPieces{
    {.name = "L300200b", .type = 5005, .shape = 2, .commands = 2, .offset = 3},
    {.name = "Cylinder", .type = kStudPieceType, .shape = 0, .commands = 1, .offset = 0},
    {.name = "ddchas0", .type = 20, .shape = 9, .commands = 0, .offset = 3},
    {.name = "L300200", .type = 5005, .shape = 2, .commands = 1, .offset = 0},
};

PieceShape TwoByThree() {
    PieceShape shape{.size = {.width = 2, .height = 3}};
    for (std::uint8_t x = 0; x < 2; ++x) {
        for (std::uint8_t y = 0; y < 3; ++y) {
            shape.cells.push_back({.top = static_cast<std::uint8_t>((x * 10) + y)});
        }
    }
    return shape;
}

std::vector<int> RotatedTops(const PieceShape& shape, unsigned rotation) {
    const CellSize footprint = shape.Footprint(rotation);
    std::vector<int> tops;
    for (int y = 0; y < footprint.height; ++y) {
        for (int x = 0; x < footprint.width; ++x) {
            tops.push_back(shape.GetCell(x, y, rotation).top);
        }
    }
    return tops;
}

}

TEST_CASE("a piece library reads its streams scaled and its shapes column by column") {
    const Result<PieceLibrary> library = ParsePieceLibrary(Library(kPieces));

    REQUIRE(library.has_value());
    CHECK(library->indices == std::vector<std::uint16_t>{0x4000, 1, 2});
    CHECK(library->positions == std::vector<Vector3>{{1, 2, -0.5F}, {0, 0, 15}});
    CHECK(library->normals == std::vector<Vector3>{{0, -1, 1}});
    CHECK(library->texCoords == std::vector<TexCoord>{{1, -0.5F}});
    REQUIRE(library->pieces.size() == 4);
    const PieceRecord& brick = library->pieces[2];
    CHECK(brick.shape.size == CellSize{.width = 2, .height = 3});
    CHECK(brick.shape.cells[3] == ShapeCell{.top = 0x8A});
    CHECK_FALSE(brick.modelOffsetPosition.has_value());
    CHECK(library->pieces[3].indexOffset == 3);
}

TEST_CASE("a chassis shape ends with the position of its model offset") {
    const Result<PieceLibrary> library = ParsePieceLibrary(Library(kPieces));

    REQUIRE(library.has_value());
    const PieceRecord& chassis = library->pieces[0];
    CHECK(chassis.IsChassis());
    CHECK(chassis.shape.cells == std::vector<ShapeCell>{{.top = 0x83, .bottom = 0xC1}});
    CHECK(chassis.modelOffsetPosition == 1);
}

TEST_CASE("a chassis model offset of 0xFFFF means it has none") {
    const Result<PieceLibrary> library =
        ParsePieceLibrary(Library({{.name = "dachas0", .type = 20, .shape = 12}}));

    REQUIRE(library.has_value());
    CHECK_FALSE(library->pieces[0].modelOffsetPosition.has_value());
}

TEST_CASE("a shape cell splits its heights from its flags") {
    const ShapeCell cell{.top = 0x83, .bottom = 0x42};

    CHECK(cell.Occupied());
    CHECK(cell.TopHeight() == 3);
    CHECK(cell.BottomHeight() == 2);
    CHECK_FALSE(cell.RestsOnBelow());
    CHECK(cell.CanSitLevel());
    CHECK_FALSE(ShapeCell{.top = 3, .bottom = 2}.CanSitLevel());
    CHECK(ShapeCell{.bottom = 0x80}.CanSitLevel());
}

TEST_CASE("pieces sort by type and command count, and are found by type or by name") {
    const Result<PieceLibrary> library = ParsePieceLibrary(Library(kPieces));

    REQUIRE(library.has_value());
    CHECK(library->pieces[1].name == "Cylinder");
    CHECK(library->Find(5005, 0) == &library->pieces[2]);
    CHECK(library->Find(5005, 1) == &library->pieces[3]);
    CHECK(library->Find(5005, 7) == &library->pieces[3]);
    CHECK(library->Find(20, 1)->name == "ddchas0");
    CHECK(library->Find(5006, 0) == nullptr);
    CHECK(library->Find("cylinder")->name == "Cylinder");
    CHECK(library->Find("DDCHAS0")->type == 20);
    CHECK(library->Find("L3002") == nullptr);
}

TEST_CASE("a rotated shape turns its footprint a quarter at a time") {
    const PieceShape shape = TwoByThree();

    CHECK(shape.Footprint(0) == CellSize{.width = 2, .height = 3});
    CHECK(shape.Footprint(1) == CellSize{.width = 3, .height = 2});
    CHECK(shape.Footprint(2) == CellSize{.width = 2, .height = 3});
    CHECK(shape.Footprint(3) == CellSize{.width = 3, .height = 2});
    CHECK(RotatedTops(shape, 0) == std::vector{0, 10, 1, 11, 2, 12});
    CHECK(RotatedTops(shape, 1) == std::vector{10, 11, 12, 0, 1, 2});
    CHECK(RotatedTops(shape, 2) == std::vector{12, 2, 11, 1, 10, 0});
    CHECK(RotatedTops(shape, 3) == std::vector{2, 1, 0, 12, 11, 10});
    CHECK(RotatedTops(shape, 5) == RotatedTops(shape, 1));
}

TEST_CASE("a piece library rejects pieces that point outside its streams") {
    CHECK_FALSE(
        ParsePieceLibrary(Library({{.name = "far", .type = 5001, .shape = 40}})).has_value());
    CHECK_FALSE(
        ParsePieceLibrary(Library({{.name = "deep", .type = 5001, .offset = 4}})).has_value());
    CHECK_FALSE(
        ParsePieceLibrary(Library({{.name = "cut", .type = 5001, .shape = 2}}, 5)).has_value());
    CHECK_FALSE(
        ParsePieceLibrary(Library({{.name = "chassis", .type = 20, .shape = 9}}, 11)).has_value());
    CHECK_FALSE(ParsePieceLibrary(Library({})).has_value());
    CHECK_FALSE(ParsePieceLibrary(Join({Library(kPieces), Stream(0x2B, 0, {})})).has_value());
}

}
