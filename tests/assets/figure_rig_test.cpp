#include "assets/figure_rig.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace rr::assets {
namespace {

std::vector<std::byte> Bytes(std::string_view text) {
    std::vector<std::byte> bytes;
    for (const char c : text) {
        bytes.push_back(static_cast<std::byte>(c));
    }
    return bytes;
}

bool SamePivots(const FigureRig& a, const FigureRig& b) {
    return std::ranges::equal(a.pivots, b.pivots, [](const RigPivot& x, const RigPivot& y) {
        return x.node == y.node && x.position == y.position;
    });
}

bool SameLimbs(const FigureRig& a, const FigureRig& b) {
    return std::ranges::equal(
        a.limbs, b.limbs, [](const RigLimb& x, const RigLimb& y) { return x.nodes == y.nodes; });
}

TEST_CASE("a written rig reads back the same") {
    const FigureRig rig{.pivots = {{.node = 2, .position = {.x = 0.25F, .y = -1.5F, .z = 2.0F}},
                                   {.node = 3, .position = {.z = 1.125F}},
                                   {.node = 5, .position = {.y = 0.1F}}},
                        .limbs = {{.nodes = {2, 3, 5}}}};

    const Result<FigureRig> read = ParseFigureRig(Bytes(WriteFigureRig(rig)));

    REQUIRE(read.has_value());
    CHECK(SamePivots(*read, rig));
    CHECK(SameLimbs(*read, rig));
}

TEST_CASE("a rig saved with a byte order mark and Windows line endings reads the same") {
    const Result<FigureRig> read = ParseFigureRig(Bytes(
        "\xEF\xBB\xBFrig 1\r\npivot 2 0 0 1\r\npivot 3 0 0 0\r\npivot 4 0 1 0\r\nlimb 2 3 4\r\n"));

    REQUIRE(read.has_value());
    CHECK(read->pivots.size() == 3);
    CHECK(read->limbs.size() == 1);
}

TEST_CASE("a rig limb needs a pivot on every node") {
    CHECK_FALSE(
        ParseFigureRig(Bytes("rig 1\npivot 2 0 0 1\npivot 3 0 0 0\nlimb 2 3 4\n")).has_value());
}

TEST_CASE("a rig limb with a segment too short to bend is rejected") {
    CHECK_FALSE(
        ParseFigureRig(Bytes("rig 1\npivot 2 0 0 1\npivot 3 0 0 1\npivot 4 0 0 0\nlimb 2 3 4\n"))
            .has_value());
}

TEST_CASE("a rig with an unknown line or version is rejected") {
    CHECK_FALSE(ParseFigureRig(Bytes("rig 1\npivot 2 0 0 1\nbone 2 3\n")).has_value());
    CHECK_FALSE(ParseFigureRig(Bytes("rig 1\npivot two 0 0 1\n")).has_value());
    CHECK_FALSE(ParseFigureRig(Bytes("pivot 2 0 0 1\n")).has_value());
    CHECK_FALSE(ParseFigureRig(Bytes("rig 2\npivot 2 0 0 1\n")).has_value());
}

}
}
