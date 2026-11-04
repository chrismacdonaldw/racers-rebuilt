#include "assets/part_library_writer.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

#include "assets/part_library.hpp"

namespace rr::assets {
namespace {

PartLibrary TwoParts() {
    return {
        .vertices = {{.position = {.x = 1.0F, .y = -2.0F, .z = 3.0F},
                      .normal = {.x = 0.0F, .y = -1.0F, .z = 0.0F},
                      .u = 0.25F,
                      .v = 0.5F},
                     {.position = {.x = 64.0F, .y = 0.0F, .z = -64.0F},
                      .normal = {.x = 1.0F, .y = 0.0F, .z = 0.0F},
                      .u = 1.0F,
                      .v = 0.0F},
                     {.position = {.x = 0.0F, .y = 5.0F, .z = 0.0F},
                      .normal = {.x = 0.0F, .y = 0.0F, .z = -1.0F},
                      .u = 0.0F,
                      .v = 1.0F}},
        .parts = {{.name = "head",
                   .scale = 0.015625F,
                   .groups = {{.material = "face", .triangles = {{0, 1, 2}}}}},
                  {.name = "hat",
                   .scale = 1.0F,
                   .groups = {{.material = "face", .triangles = {{2, 1, 0}}},
                              {.material = "bb_hat", .triangles = {{0, 2, 1}, {1, 2, 0}}}}}},
    };
}

constexpr float kRounding = 0.01F;

bool IsNear(float read, float written) {
    return std::abs(read - written) < kRounding;
}

bool IsSameVertex(const GeometryVertex& read, const GeometryVertex& written) {
    return read.position == written.position && IsNear(read.normal.x, written.normal.x) &&
           IsNear(read.normal.y, written.normal.y) && IsNear(read.normal.z, written.normal.z) &&
           IsNear(read.u, written.u) && IsNear(read.v, written.v);
}

bool IsSameGroup(const PartGroup& read, const PartGroup& written) {
    return read.material == written.material && read.triangles == written.triangles;
}

bool IsSamePart(const LibraryPart& read, const LibraryPart& written) {
    return read.name == written.name && read.scale == written.scale &&
           std::ranges::equal(read.groups, written.groups, IsSameGroup);
}

TEST_CASE("a written part library reads back the same") {
    const PartLibrary library = TwoParts();

    const Result<PartLibrary> read = WritePartLibrary(library).and_then(
        [](const std::vector<std::byte>& bytes) { return ParsePartLibrary(bytes); });

    REQUIRE(read.has_value());
    CHECK(std::ranges::equal(read->vertices, library.vertices, IsSameVertex));
    CHECK(std::ranges::equal(read->parts, library.parts, IsSamePart));
}

TEST_CASE("a pool value past 16 bits isn't written") {
    PartLibrary library = TwoParts();
    library.vertices[0].position.x = 40000.0F;

    CHECK_FALSE(WritePartLibrary(library).has_value());
}

}
}
