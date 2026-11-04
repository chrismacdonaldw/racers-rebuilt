#include "assets/derived_shape.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

#include "assets/ldraw_extra_pieces.hpp"
#include "assets/ldraw_part.hpp"

namespace rr::assets {
namespace {

void AddBox(GeometryAsset& model, const Vector3& low, const Vector3& high) {
    const auto base = static_cast<std::uint32_t>(model.vertices.size());
    for (int corner = 0; corner < 8; ++corner) {
        model.vertices.push_back({.position = {.x = (corner & 1) != 0 ? high.x : low.x,
                                               .y = (corner & 2) != 0 ? high.y : low.y,
                                               .z = (corner & 4) != 0 ? high.z : low.z}});
    }
    constexpr std::array<std::uint32_t, 36> kFaces{0, 1, 3, 0, 3, 2, 4, 5, 7, 4, 7, 6,
                                                   0, 1, 5, 0, 5, 4, 2, 3, 7, 2, 7, 6,
                                                   0, 2, 6, 0, 6, 4, 1, 3, 7, 1, 7, 5};
    for (const std::uint32_t corner : kFaces) {
        model.indices.push_back(base + corner);
    }
}

bool DerivesItsCells(LdrawLibrary& library, std::span<const LdrawPart> part,
                     const PieceShape& expected) {
    const Result<GeometryAsset> model = LoadLdrawPiece(library, part);
    if (!model) {
        return false;
    }
    const PieceShape derived = DeriveShape(*model);
    return derived.size == expected.size && derived.cells == expected.cells;
}

TEST_CASE("a tile with one stud has one occupied cell, both resting a plate high") {
    GeometryAsset tile;
    AddBox(tile, {}, {.x = 1.0F, .y = 2.0F, .z = 0.4F});
    AddBox(tile, {.x = 0.3F, .y = 0.3F, .z = 0.4F}, {.x = 0.7F, .y = 0.7F, .z = 0.6F});

    const PieceShape shape = DeriveShape(tile);

    CHECK(shape.size == CellSize{.width = 1, .height = 2});
    CHECK(shape.cells ==
          std::vector<ShapeCell>{{.top = 0x81, .bottom = 0x80}, {.top = 0x01, .bottom = 0x80}});
}

TEST_CASE("a ledge raised off the ground hangs over its cell without resting") {
    GeometryAsset ledge;
    AddBox(ledge, {}, {.x = 1.0F, .y = 1.0F, .z = 1.2F});
    AddBox(ledge, {.x = 1.0F, .z = 0.8F}, {.x = 1.5F, .y = 1.0F, .z = 1.2F});

    const PieceShape shape = DeriveShape(ledge);

    CHECK(shape.size == CellSize{.width = 2, .height = 1});
    CHECK(shape.cells ==
          std::vector<ShapeCell>{{.top = 0x03, .bottom = 0x80}, {.top = 0x03, .bottom = 0x02}});
}

TEST_CASE("every extra piece's cells are the ones its LDraw part derives") {
    LdrawLibrary library(RR_LDRAW_DIR);
    const std::vector<PieceRecord> records = ExtraPieceRecords();
    const std::span<const LdrawPart> parts = ExtraPieceParts();
    REQUIRE(records.size() == parts.size());

    for (std::size_t piece = 0; piece < parts.size(); ++piece) {
        CAPTURE(parts[piece].file);
        CHECK(DerivesItsCells(library, parts.subspan(piece, 1), records[piece].shape));
    }
}

}
}
