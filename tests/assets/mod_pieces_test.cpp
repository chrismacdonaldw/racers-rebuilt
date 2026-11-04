#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "archive_survey.hpp"
#include "assets/derived_shape.hpp"
#include "assets/folder_layer.hpp"
#include "assets/geometry_writer.hpp"
#include "assets/ldraw_extra_pieces.hpp"
#include "assets/piece_catalog.hpp"
#include "temp_folder.hpp"

namespace rr::assets {
namespace {

GeometryAsset Brick(float height = 1.2F) {
    GeometryAsset brick{.materials = {"piece"}};
    for (int corner = 0; corner < 8; ++corner) {
        brick.vertices.push_back({.position = {.x = (corner & 1) != 0 ? 2.0F : 0.0F,
                                               .y = (corner & 2) != 0 ? 1.0F : 0.0F,
                                               .z = (corner & 4) != 0 ? height : 0.0F}});
    }
    brick.indices = {0, 1, 3, 0, 3, 2, 4, 5, 7, 4, 7, 6, 0, 1, 5, 0, 5, 4,
                     2, 3, 7, 2, 7, 6, 0, 2, 6, 0, 6, 4, 1, 3, 7, 1, 7, 5};
    brick.ranges = {{.indexCount = static_cast<std::uint32_t>(brick.indices.size())}};
    return brick;
}

std::optional<Vfs> RetailWithPieces(const test::TempFolder& mod, std::string_view table,
                                    const GeometryAsset& brick = Brick()) {
    std::optional<Vfs> vfs = test::OpenRetailVfs();
    if (!vfs) {
        return std::nullopt;
    }
    const std::filesystem::path folder = mod.Path() / "MENUDATA" / "PIECEDB";
    std::filesystem::create_directories(folder / "MODELS");
    std::ofstream(folder / "PIECES.TOML", std::ios::binary) << table;
    const Result<std::vector<std::byte>> model = WriteGeometry(brick, GdbVertices::Lit);
    REQUIRE(model.has_value());
    std::ofstream(folder / "MODELS" / "LAVA-BRICK.GDB", std::ios::binary)
        .write(reinterpret_cast<const char*>(model->data()),
               static_cast<std::streamsize>(model->size()));
    Result<FolderLayer> layer = FolderLayer::Scan(mod.Path());
    REQUIRE(layer.has_value());
    vfs->Mount(std::make_unique<FolderLayer>(std::move(*layer)));
    return vfs;
}

std::size_t OfferedColours(const PieceCatalog& catalog, std::string_view set,
                           const PieceRecord& piece) {
    const auto found = std::ranges::find(catalog.partSets, set, &CatalogPartSet::name);
    if (found == catalog.partSets.end()) {
        return 0;
    }
    return static_cast<std::size_t>(
        std::ranges::count_if(found->choices, [&](const PartChoice& choice) {
            return &catalog.library.pieces.at(choice.piece) == &piece;
        }));
}

std::optional<PieceCatalog> LoadWithLavaBrick(const test::TempFolder& mod,
                                              std::vector<std::string>& problems) {
    const std::optional<Vfs> vfs =
        RetailWithPieces(mod, "[[piece]]\nname = \"lava-brick\"\nset = \"dp_cset\"\n");
    if (!vfs) {
        return std::nullopt;
    }
    Result<PieceCatalog> catalog = PieceCatalog::Load(*vfs, PieceDetail::Menu, problems);
    REQUIRE(catalog.has_value());
    return std::move(*catalog);
}

TEST_CASE("a mod piece takes a type of its own and its shape from its model") {
    const test::TempFolder mod;
    std::vector<std::string> problems;
    const std::optional<PieceCatalog> catalog = LoadWithLavaBrick(mod, problems);
    if (!catalog) {
        return;
    }

    const PieceRecord* brick = catalog->library.Find("LAVA-BRICK");

    REQUIRE(brick != nullptr);
    CHECK(brick->type >= kFirstModPieceType);
    CHECK(brick->shape.size == CellSize{.width = 2, .height = 1});
    CHECK(catalog->ModelOf(*brick) != nullptr);
}

TEST_CASE("a mod piece without colours joins its part set in every colour") {
    const test::TempFolder mod;
    std::vector<std::string> problems;
    const std::optional<PieceCatalog> catalog = LoadWithLavaBrick(mod, problems);
    if (!catalog) {
        return;
    }

    const PieceRecord* brick = catalog->library.Find("LAVA-BRICK");

    REQUIRE(brick != nullptr);
    CHECK(problems.empty());
    CHECK(OfferedColours(*catalog, "DP_CSET", *brick) == catalog->colours.size());
}

TEST_CASE("a later mod piece replaces an earlier one, and broken ones are left out with reasons") {
    const test::TempFolder mod;
    const std::optional<Vfs> vfs = RetailWithPieces(
        mod,
        "[[piece]]\nname = \"ghost\"\nset = \"dp_cset\"\n"
        "[[piece]]\nname = \"lava-brick\"\nset = \"nosuch\"\n"
        "[[piece]]\nname = \"LAVA-BRICK\"\nset = \"dp_cset\"\ncolours = [\"plaid\"]\n");
    if (!vfs) {
        return;
    }
    std::vector<std::string> problems;

    const Result<PieceCatalog> catalog = PieceCatalog::Load(*vfs, PieceDetail::Menu, problems);

    REQUIRE(catalog.has_value());
    CHECK(problems ==
          std::vector<std::string>{"MENUDATA/PIECEDB/PIECES.TOML: piece ghost: there's no model at "
                                   "MENUDATA/PIECEDB/MODELS/GHOST.GDB",
                                   "MENUDATA/PIECEDB/PIECES.TOML: piece LAVA-BRICK: there's no "
                                   "colour plaid"});
}

TEST_CASE("a mod piece whose model leaves the build grid is left out") {
    const test::TempFolder mod;
    const std::optional<Vfs> vfs = RetailWithPieces(
        mod, "[[piece]]\nname = \"lava-brick\"\nset = \"dp_cset\"\n", Brick(30.0F));
    if (!vfs) {
        return;
    }
    std::vector<std::string> problems;

    const Result<PieceCatalog> catalog = PieceCatalog::Load(*vfs, PieceDetail::Menu, problems);

    REQUIRE(catalog.has_value());
    CHECK(catalog->library.Find("lava-brick") == nullptr);
    CHECK(problems == std::vector<std::string>{"MENUDATA/PIECEDB/PIECES.TOML: piece lava-brick: "
                                               "its model must fit in 10 by 6 cells and 63 plates "
                                               "from the origin"});
}

TEST_CASE("model bounds reject vertices that aren't finite or leave the grid") {
    GeometryAsset unbounded = Brick();
    unbounded.vertices[1].position.x = std::numeric_limits<float>::quiet_NaN();
    GeometryAsset wide = Brick();
    wide.vertices[1].position.x = 10.5F;
    GeometryAsset dangling = Brick();
    dangling.indices.push_back(8);

    CHECK(IsOnBuildGrid(Brick()));
    CHECK_FALSE(IsOnBuildGrid(unbounded));
    CHECK_FALSE(IsOnBuildGrid(wide));
    CHECK_FALSE(IsOnBuildGrid(dangling));
}

TEST_CASE("piece types count a mod's pieces without reading their models") {
    const test::TempFolder mod;
    const std::optional<Vfs> vfs =
        RetailWithPieces(mod, "[[piece]]\nname = \"lava-brick\"\nset = \"dp_cset\"\n");
    if (!vfs) {
        return;
    }

    const Result<PieceCatalog> catalog = PieceCatalog::Load(*vfs, PieceDetail::Menu);
    const Result<std::vector<std::uint16_t>> types = LoadPieceTypes(*vfs);

    REQUIRE(catalog.has_value());
    REQUIRE(types.has_value());
    CHECK(types->size() == catalog->library.pieces.size());
    CHECK(std::ranges::contains(*types, catalog->library.Find("lava-brick")->type));
}

}
}
