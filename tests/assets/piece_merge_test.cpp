#include "assets/piece_merge.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "assets/ldraw_extra_pieces.hpp"

namespace rr::assets {
namespace {

PieceRecord Piece(std::string name, std::uint16_t type, std::uint32_t commandCount,
                  std::uint32_t indexOffset) {
    return {.name = std::move(name),
            .type = type,
            .shape = {},
            .modelOffsetPosition = std::nullopt,
            .commandCount = commandCount,
            .indexOffset = indexOffset};
}

PieceLayer GameLayer() {
    PieceLayer game;
    game.library.pieces = {Piece("brick", kFirstBrickType, 1, 0)};
    game.library.indices = {0x0003, 0, 0, 1, 1};
    game.library.positions.resize(2);
    game.library.normals.resize(1);
    game.materials = {"unassign", "bottom", "nub", "red"};
    return game;
}

PieceLayer ModLayer() {
    PieceLayer mod;
    mod.library.pieces = {Piece("wing", 0x2000, 2, 0), Piece("BRICK", kFirstBrickType, 1, 14),
                          Piece("clash", kFirstBrickType, 1, 14), Piece("chassis", 0x10, 1, 14),
                          Piece("broken", 0x2001, 1, 19)};
    mod.library.indices = {0x5003, 0, 1,      0, 1, 0, 0, 2,      1, 0, 0x2000, 2,
                           1,      0, 0x0004, 0, 0, 1, 2, 0x0003, 9, 0, 0,      0};
    mod.library.positions.resize(3);
    mod.library.normals.resize(2);
    mod.library.texCoords.resize(1);
    mod.materials = {"unassign", "bottom", "nub", "gold", "Red"};
    return mod;
}

std::vector<std::uint16_t> Commands(const PieceLibrary& library, const PieceRecord& piece,
                                    std::size_t count) {
    const std::span<const std::uint16_t> indices = library.indices;
    const std::span<const std::uint16_t> commands = indices.subspan(piece.indexOffset, count);
    return {commands.begin(), commands.end()};
}

}

TEST_CASE("a mod's pieces join the game's with their pools and materials remapped") {
    std::vector<std::string> problems;
    const PieceLayer merged = MergePieceLayers({GameLayer(), ModLayer()}, problems);
    const PieceLibrary& library = merged.library;

    CHECK(problems == std::vector<std::string>{
                          "mod piece clash: another piece has type 0x1389",
                          "mod piece chassis: a new piece needs a type from 0x1389 to 0x2fff",
                          "mod piece broken: its commands read past its library"});
    CHECK(merged.materials == std::vector<std::string>{"unassign", "bottom", "nub", "red", "gold"});
    CHECK(library.positions.size() == 5);
    CHECK(library.normals.size() == 3);
    CHECK(library.texCoords.size() == 1);
    REQUIRE(library.pieces.size() == 2);
    CHECK(library.pieces[0].name == "BRICK");
    CHECK(Commands(library, library.pieces[0], 5) ==
          std::vector<std::uint16_t>{0x0003, 2, 1, 3, 4});
    CHECK(library.pieces[1].name == "wing");
    CHECK(Commands(library, library.pieces[1], 14) ==
          std::vector<std::uint16_t>{0x5004, 2, 2, 0, 3, 1, 0, 4, 2, 0, 0x2000, 4, 2, 0});
}

TEST_CASE("a mod without its own materials indexes the game's") {
    PieceLayer mod = ModLayer();
    mod.materials.clear();
    mod.library.pieces = {Piece("BRICK", kFirstBrickType, 1, 0)};
    mod.library.indices = {0x0003, 0, 0, 1, 2};

    std::vector<std::string> problems;
    const PieceLayer merged = MergePieceLayers({GameLayer(), std::move(mod)}, problems);

    CHECK(merged.materials.size() == 4);
    REQUIRE(merged.library.pieces.size() == 1);
    CHECK(Commands(merged.library, merged.library.pieces[0], 1) ==
          std::vector<std::uint16_t>{0x0003});
}

TEST_CASE("a mod piece with a type the port gives its own pieces is left out") {
    PieceLayer mod = ModLayer();
    mod.library.pieces = {Piece("TALL", kFirstExtraPieceType, 1, 0)};
    mod.library.indices = {0x0003, 0, 0, 1, 2};
    std::vector<std::string> problems;

    const PieceLayer merged = MergePieceLayers({GameLayer(), std::move(mod)}, problems);

    CHECK(merged.library.pieces.size() == 1);
    CHECK(problems == std::vector<std::string>{
                          "mod piece TALL: a new piece needs a type from 0x1389 to 0x2fff"});
}

}
