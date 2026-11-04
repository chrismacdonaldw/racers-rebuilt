#include "assets/ldraw_minifig.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <array>
#include <span>
#include <string_view>

#include "assets/ldraw_part.hpp"

namespace rr::assets {
namespace {

constexpr std::array<std::string_view, 5> kBodies{"rr", "hr", "rp", "hp", "leg_box"};

bool StartsWithTorso(std::string_view body) {
    const std::span<const MinifigPiece> figure = MinifigFigureOf(body);
    return !figure.empty() && figure.front().file == "973.dat" &&
           figure.front().material == "torso";
}

bool HasFile(std::string_view body, std::string_view file) {
    return std::ranges::any_of(MinifigFigureOf(body),
                               [&](const MinifigPiece& piece) { return piece.file == file; });
}

TEST_CASE("every driver body has a figure that starts with the torso") {
    for (const std::string_view body : kBodies) {
        CAPTURE(body);
        CHECK(StartsWithTorso(body));
    }
    CHECK(MinifigFigureOf("LEG_BOX").data() == MinifigFigureOf("leg_box").data());
    CHECK(MinifigFigureOf("heads").empty());
}

TEST_CASE("hook and peg leg bodies swap in those parts") {
    for (const std::string_view body : {"rr", "hr", "rp", "hp"}) {
        CAPTURE(body);
        CHECK(HasFile(body, "2531.dat") == body.starts_with('h'));
        CHECK(HasFile(body, "2532.dat") == body.ends_with('p'));
    }
}

TEST_CASE("every figure part and the head ship in the LDraw folder") {
    LdrawLibrary library(RR_LDRAW_DIR);
    for (const std::string_view body : kBodies) {
        for (const MinifigPiece& piece : MinifigFigureOf(body)) {
            CAPTURE(piece.file);
            CHECK(library.LoadPart(piece.file).has_value());
        }
    }
    CHECK(library.LoadPart(kMinifigHeadFile).has_value());
}

}
}
