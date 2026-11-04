#include "assets/piece_table.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "assets/ldraw_extra_pieces.hpp"

namespace rr::assets {
namespace {

std::string ErrorFor(std::string_view text) {
    const Result<std::vector<ModPiece>> pieces = ParsePieceTable(text, "PIECES.TOML");
    REQUIRE_FALSE(pieces.has_value());
    return pieces.error().message;
}

TEST_CASE("a piece table reads every key") {
    const Result<std::vector<ModPiece>> pieces = ParsePieceTable(
        "[[piece]]\nname = \"lava-brick\"\nset = \"dp_cset\"\ncolours = [\"red\", \"black\"]\n"
        "[[piece]]\nname = \"LAVA_2\"\nset = \"dp_cset\"\n",
        "PIECES.TOML");

    REQUIRE(pieces.has_value());
    CHECK(*pieces == std::vector<ModPiece>{
                         {.name = "lava-brick", .set = "dp_cset", .colours = {"red", "black"}},
                         {.name = "LAVA_2", .set = "dp_cset"}});
}

TEST_CASE("a piece table rejects bad names, sets and colours") {
    const std::vector<std::pair<std::string_view, std::string_view>> cases{
        {"[[piece]]\nname = \"lava brick\"\nset = \"a\"\n",
         "PIECES.TOML: piece 1: name must be letters, digits, - or _"},
        {"[[piece]]\nname = \"lava\"\n", "PIECES.TOML: piece 1: set must name a part set"},
        {"[[piece]]\nname = \"lava\"\nset = \"a\"\ncolours = []\n",
         "PIECES.TOML: piece 1: colours must list 1 to 64 colour names"},
        {"piece = 3\n", "PIECES.TOML: piece must be [[piece]] tables"}};

    for (const auto& [text, error] : cases) {
        CAPTURE(text);
        CHECK(ErrorFor(text) == error);
    }
}

TEST_CASE("a mod piece's type comes from its name in any case, past the types taken") {
    const std::vector<ModPiece> pieces{{.name = "lava-brick"}, {.name = "LAVA-BRICK"}};

    const std::vector<std::uint16_t> types = ModPieceTypes({}, pieces);

    REQUIRE(types.size() == 2);
    CHECK(types[0] >= kFirstModPieceType);
    CHECK(types[1] == (types[0] == 0xFFFF ? kFirstModPieceType : types[0] + 1));
    const PieceLibrary taken{.pieces = {{.name = "OTHER", .type = types[0]}}};
    CHECK(ModPieceTypes(taken, std::vector<ModPiece>{{.name = "Lava-Brick"}}) ==
          std::vector<std::uint16_t>{types[1]});
}

}
}
