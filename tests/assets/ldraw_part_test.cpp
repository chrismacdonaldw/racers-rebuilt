#include "assets/ldraw_part.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <string>
#include <string_view>
#include <vector>

#include "temp_folder.hpp"

namespace rr::assets {
namespace {

constexpr float kTolerance = 0.0001F;

void WriteFile(const std::filesystem::path& path, std::string_view text) {
    std::filesystem::create_directories(path.parent_path());
    std::ofstream(path, std::ios::binary) << text;
}

std::vector<Vector3> Positions(const GeometryAsset& geometry) {
    std::vector<Vector3> positions;
    positions.reserve(geometry.vertices.size());
    for (const GeometryVertex& vertex : geometry.vertices) {
        positions.push_back(vertex.position);
    }
    return positions;
}

std::string MaterialOf(const GeometryAsset& geometry, std::size_t range) {
    return geometry.materials.at(geometry.ranges.at(range).material);
}

bool IsNear(const Vector3& a, const Vector3& b) {
    return Length(a - b) < kTolerance;
}

TEST_CASE("a sub-file is moved by its matrix and its main colour takes the reference's colour") {
    const test::TempFolder folder;
    WriteFile(folder.Path() / "parts/top.dat", "1 4 20 0 0 1 0 0 0 1 0 0 0 1 s\\side.dat\n");
    WriteFile(folder.Path() / "parts/s/side.dat",
              "3 16 0 0 0 20 0 0 0 0 20\n3 2 0 0 0 0 -20 0 20 0 0\n");
    LdrawLibrary library(folder.Path());

    const Result<GeometryAsset> part = library.LoadPart("top.dat");

    REQUIRE(part.has_value());
    REQUIRE(part->ranges.size() == 2);
    CHECK(MaterialOf(*part, 0) == "2");
    CHECK(MaterialOf(*part, 1) == "4");
    const std::vector<Vector3> positions = Positions(*part);
    CHECK(IsNear(positions[4], {.x = 2.0F}));
    CHECK(IsNear(positions[5], {.x = 1.0F, .y = 1.0F}));
}

TEST_CASE("names resolve in parts, then parts/s, then p, in any case and with either slash") {
    const test::TempFolder folder;
    WriteFile(folder.Path() / "parts/top.dat",
              "1 16 0 0 0 1 0 0 0 1 0 0 0 1 SAME.DAT\n1 16 0 0 0 1 0 0 0 1 0 0 0 1 48/prim.dat\n");
    WriteFile(folder.Path() / "parts/same.dat", "3 1 0 0 0 20 0 0 0 0 20\n");
    WriteFile(folder.Path() / "p/same.dat", "3 9 0 0 0 20 0 0 0 0 20\n");
    WriteFile(folder.Path() / "p/48/prim.dat", "3 3 0 0 0 20 0 0 0 0 20\n");
    LdrawLibrary library(folder.Path());

    const Result<GeometryAsset> part = library.LoadPart("top.dat");

    REQUIRE(part.has_value());
    CHECK(part->materials == std::vector<std::string>{"1", "3"});
    CHECK(library.LoadPart("missing.dat").error().code == ErrorCode::NotFound);
}

TEST_CASE("a mirroring matrix or INVERTNEXT keeps faces pointing out") {
    const test::TempFolder folder;
    WriteFile(folder.Path() / "parts/face.dat", "3 16 0 0 0 20 0 0 0 0 20\n");
    WriteFile(folder.Path() / "parts/mirrored.dat", "1 16 0 0 0 -1 0 0 0 1 0 0 0 1 face.dat\n");
    WriteFile(folder.Path() / "parts/inverted.dat",
              "0 BFC INVERTNEXT\n1 16 0 0 0 1 0 0 0 1 0 0 0 1 face.dat\n");
    LdrawLibrary library(folder.Path());

    const Result<GeometryAsset> plain = library.LoadPart("face.dat");
    const Result<GeometryAsset> mirrored = library.LoadPart("mirrored.dat");
    const Result<GeometryAsset> inverted = library.LoadPart("inverted.dat");

    REQUIRE(plain.has_value());
    REQUIRE(mirrored.has_value());
    REQUIRE(inverted.has_value());
    CHECK(IsNear(plain->vertices[0].normal, {.z = 1.0F}));
    CHECK(IsNear(mirrored->vertices[0].normal, {.z = 1.0F}));
    CHECK(IsNear(inverted->vertices[0].normal, {.z = -1.0F}));
}

TEST_CASE("normals smooth across a shallow fold and stay hard across a sharp one") {
    const test::TempFolder folder;
    WriteFile(folder.Path() / "parts/shallow.dat",
              "3 16 0 0 0 20 0 0 0 0 20\n3 16 20 0 0 40 -3 0 20 0 20\n");
    WriteFile(folder.Path() / "parts/sharp.dat",
              "3 16 0 0 0 20 0 0 0 0 20\n3 16 20 0 0 20 -20 0 20 0 20\n");
    LdrawLibrary library(folder.Path());

    const Result<GeometryAsset> shallow = library.LoadPart("shallow.dat");
    const Result<GeometryAsset> sharp = library.LoadPart("sharp.dat");

    REQUIRE(shallow.has_value());
    REQUIRE(sharp.has_value());
    CHECK(IsNear(shallow->vertices[1].normal, shallow->vertices[3].normal));
    CHECK_FALSE(IsNear(sharp->vertices[1].normal, sharp->vertices[3].normal));
    CHECK(IsNear(sharp->vertices[1].normal, {.z = 1.0F}));
}

TEST_CASE("sub-files nested past the depth cap or past the triangle cap fail") {
    const test::TempFolder folder;
    WriteFile(folder.Path() / "parts/loop.dat",
              "3 16 0 0 0 20 0 0 0 0 20\n1 16 0 0 0 1 0 0 0 1 0 0 0 1 loop.dat\n");
    LdrawLibrary library(folder.Path());

    const Result<GeometryAsset> nested = library.LoadPart("loop.dat");
    const Result<GeometryAsset> crowded = library.LoadPart("loop.dat", {.maxTriangles = 3});

    REQUIRE_FALSE(nested.has_value());
    CHECK(nested.error().code == ErrorCode::InvalidData);
    REQUIRE_FALSE(crowded.has_value());
    CHECK(crowded.error().message.find("over 3 triangles") != std::string::npos);
}

TEST_CASE("a 2 x 4 brick is 4 by 2 units and 1.2 high, studs up, in game axes") {
    LdrawLibrary library(RR_LDRAW_DIR);

    const Result<GeometryAsset> brick = library.LoadPart("3001.dat");

    REQUIRE(brick.has_value());
    Vector3 low = brick->vertices.front().position;
    Vector3 high = low;
    for (const GeometryVertex& vertex : brick->vertices) {
        low = {.x = std::min(low.x, vertex.position.x),
               .y = std::min(low.y, vertex.position.y),
               .z = std::min(low.z, vertex.position.z)};
        high = {.x = std::max(high.x, vertex.position.x),
                .y = std::max(high.y, vertex.position.y),
                .z = std::max(high.z, vertex.position.z)};
    }
    CHECK(IsNear(low, {.x = -2.0F, .y = -1.0F, .z = -1.2F}));
    CHECK(IsNear(high, {.x = 2.0F, .y = 1.0F, .z = 0.2F}));
    CHECK(brick->materials == std::vector<std::string>{"16"});
}

}
}
