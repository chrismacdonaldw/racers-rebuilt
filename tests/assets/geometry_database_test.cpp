#include "assets/geometry_database.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <vector>

#include "assets/token_bytes.hpp"

namespace rr::assets {
namespace {

using test::Bytes;
using test::Floats;
using test::Ints;
using test::Join;
using test::ListHeader;

std::vector<std::byte> TwoMaterials() {
    return Join({Bytes({0x27}), ListHeader(2), Bytes({0x02, 'a', 0x00, 0x02, 'b', 0x00, 0x06})});
}

std::vector<std::byte> ThreeColoredVertices() {
    return Join({
        Bytes({0x2A}),
        ListHeader(3),
        Floats({0, 0, 0, 0, 1}),
        Ints({10, 20, 30, 40}),
        Floats({1, 0, 0, 1, 1}),
        Ints({50, 60, 70, 80}),
        Floats({0, 1, 0, 0, 0}),
        Ints({90, 100, 110, 120}),
        Bytes({0x06}),
    });
}

std::vector<std::byte> TwoTriangles() {
    return Join({Bytes({0x2D}), ListHeader(2), Ints({4, 5, 6, 6, 5, 4}), Bytes({0x06})});
}

std::vector<std::byte> DrawList(std::uint8_t count, const std::vector<std::byte>& commands) {
    return Join({Bytes({0x2E}), ListHeader(count), commands, Bytes({0x06})});
}

Result<GeometryAsset> ParseModel(std::uint8_t count, const std::vector<std::byte>& commands) {
    return ParseGeometry(
        Join({TwoMaterials(), ThreeColoredVertices(), TwoTriangles(), DrawList(count, commands)}));
}

std::vector<std::byte> LoadIntoSlot4() {
    return Join({Bytes({0x31}), Ints({4, 0, 3})});
}

TEST_CASE("triangles draw through the vertex cache") {
    const Result<GeometryAsset> geometry = ParseGeometry(Join({
        TwoMaterials(),
        ThreeColoredVertices(),
        TwoTriangles(),
        DrawList(5, Join({Bytes({0x32}), Ints({7}), LoadIntoSlot4(), Bytes({0x2D}), Ints({0, 1}),
                          Bytes({0x27}), Ints({1}), Bytes({0x2D}), Ints({1, 1})})),
        Bytes({0x33}),
        Floats({2}),
    }));

    REQUIRE(geometry.has_value());
    CHECK(geometry->materials == std::vector<std::string>{"a", "b"});
    CHECK(geometry->indices == std::vector<std::uint32_t>{0, 1, 2, 2, 1, 0});
    CHECK(geometry->ranges == std::vector<GeometryRange>{
                                  {.firstIndex = 0, .indexCount = 3, .material = 0, .joint = 7},
                                  {.firstIndex = 3, .indexCount = 3, .material = 1, .joint = 7}});
    CHECK(geometry->scale == 2.0F);
    REQUIRE(geometry->vertices.size() == 3);
    const GeometryVertex& vertex = geometry->vertices[1];
    CHECK(vertex.position == Vector3{.x = 2.0F});
    CHECK(vertex.u == 1.0F);
    CHECK(vertex.v == 1.0F);
    CHECK(vertex.rgba == Rgba8{50, 60, 70, 80});
    CHECK(vertex.joint == 7);
}

TEST_CASE("draws with the same material share a range") {
    const Result<GeometryAsset> geometry = ParseModel(
        3, Join({LoadIntoSlot4(), Bytes({0x2D}), Ints({0, 1}), Bytes({0x2D}), Ints({1, 1})}));

    REQUIRE(geometry.has_value());
    CHECK(geometry->ranges ==
          std::vector<GeometryRange>{{.firstIndex = 0, .indexCount = 6, .material = 0}});
}

TEST_CASE("triangles under another joint start a new range") {
    const Result<GeometryAsset> geometry =
        ParseModel(4, Join({Bytes({0x31}), Ints({5, 1, 2}), Bytes({0x32}), Ints({2}), Bytes({0x31}),
                            Ints({4, 0, 1}), Bytes({0x2D}), Ints({0, 2})}));

    REQUIRE(geometry.has_value());
    CHECK(geometry->ranges == std::vector<GeometryRange>{
                                  {.firstIndex = 0, .indexCount = 3, .material = 0, .joint = 2},
                                  {.firstIndex = 3, .indexCount = 3, .material = 0, .joint = 0}});
}

TEST_CASE("a corner on an empty cache slot fails") {
    const std::vector<std::byte> loadIntoSlot0 = Join({Bytes({0x31}), Ints({0, 0, 3})});

    CHECK_FALSE(ParseModel(2, Join({loadIntoSlot0, Bytes({0x2D}), Ints({0, 1})})).has_value());
}

TEST_CASE("loads must fit the cache and the vertex list") {
    CHECK_FALSE(ParseModel(1, Join({Bytes({0x31}), Ints({62, 0, 3})})).has_value());
    CHECK_FALSE(ParseModel(1, Join({Bytes({0x31}), Ints({0, 1, 3})})).has_value());
    CHECK(ParseModel(1, Join({Bytes({0x31}), Ints({61, 0, 3})})).has_value());
}

TEST_CASE("draws must fit the triangle list") {
    CHECK_FALSE(ParseModel(2, Join({LoadIntoSlot4(), Bytes({0x2D}), Ints({1, 2})})).has_value());
}

TEST_CASE("a vertex loaded under two matrices fails") {
    const std::vector<std::byte> reload = Join({LoadIntoSlot4(), LoadIntoSlot4()});
    const std::vector<std::byte> reloadUnderMatrix1 =
        Join({LoadIntoSlot4(), Bytes({0x32}), Ints({1}), LoadIntoSlot4()});

    CHECK(ParseModel(2, reload).has_value());
    CHECK_FALSE(ParseModel(3, reloadUnderMatrix1).has_value());
}

TEST_CASE("a material past the names fails") {
    CHECK_FALSE(ParseModel(1, Join({Bytes({0x27}), Ints({2})})).has_value());
}

TEST_CASE("pushed matrices tag loads until popped") {
    const Result<GeometryAsset> geometry = ParseModel(
        6, Join({Bytes({0x2F}), Ints({3}), Bytes({0x31}), Ints({0, 0, 1}), Bytes({0x2F}), Ints({5}),
                 Bytes({0x31}), Ints({1, 1, 1}), Bytes({0x30}), Bytes({0x31}), Ints({2, 2, 1})}));

    REQUIRE(geometry.has_value());
    CHECK(geometry->vertices[0].joint == 3);
    CHECK(geometry->vertices[1].joint == 5);
    CHECK(geometry->vertices[2].joint == 3);
}

TEST_CASE("popping an empty matrix stack fails") {
    CHECK_FALSE(ParseModel(1, Bytes({0x30})).has_value());
}

TEST_CASE("lit and bare vertices fill what they carry") {
    const Result<GeometryAsset> lit = ParseGeometry(
        Join({Bytes({0x29}), ListHeader(1), Floats({1, 2, 3, 4, 5, 6, 7, 8}), Bytes({0x06})}));
    const Result<GeometryAsset> bare =
        ParseGeometry(Join({Bytes({0x34}), ListHeader(1), Floats({1, 2, 3}), Bytes({0x06})}));

    REQUIRE(lit.has_value());
    REQUIRE(bare.has_value());
    CHECK(lit->vertices.front().normal == Vector3{.x = 6.0F, .y = 7.0F, .z = 8.0F});
    CHECK(lit->vertices.front().rgba == kOpaqueWhite);
    CHECK(bare->vertices.front().position == Vector3{.x = 1.0F, .y = 2.0F, .z = 3.0F});
    CHECK(bare->vertices.front().u == 0.0F);
}

TEST_CASE("a second vertex list fails") {
    const std::vector<std::byte> bareVertex =
        Join({Bytes({0x34}), ListHeader(1), Floats({1, 2, 3}), Bytes({0x06})});

    CHECK_FALSE(ParseGeometry(Join({bareVertex, bareVertex})).has_value());
}

TEST_CASE("a model may hold only material names") {
    const Result<GeometryAsset> geometry = ParseGeometry(TwoMaterials());

    REQUIRE(geometry.has_value());
    CHECK(geometry->materials.size() == 2);
    CHECK(geometry->vertices.empty());
    CHECK(geometry->ranges.empty());
}

}
}
