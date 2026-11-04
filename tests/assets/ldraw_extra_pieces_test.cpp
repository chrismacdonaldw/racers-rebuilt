#include "assets/ldraw_extra_pieces.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

#include "assets/game_path.hpp"

namespace rr::assets {
namespace {

bool FitsSomeWay(const PieceShape& shape) {
    return shape.FitsWithin(kBuildGridSize) ||
           (shape.size.height <= kBuildGridSize.width && shape.size.width <= kBuildGridSize.height);
}

bool IsMapped(std::string_view name) {
    return std::ranges::any_of(LdrawParts(), [&](const LdrawPart& part) {
        return ToUpperAscii(part.piece) == ToUpperAscii(name);
    });
}

bool IsListedAt(const PieceRecord& record, std::size_t piece) {
    return record.type == kFirstExtraPieceType + piece && IsExtraPieceType(record.type) &&
           record.commandCount == 0 && ExtraPieceParts()[piece].piece == record.name;
}

TEST_CASE("extra pieces take fixed types from 0x3000 and names no other piece has") {
    const std::vector<PieceRecord> records = ExtraPieceRecords();
    REQUIRE_FALSE(records.empty());
    std::vector<std::string> names;
    std::string misfits;
    for (std::size_t piece = 0; piece < records.size(); ++piece) {
        if (!IsListedAt(records[piece], piece) || IsMapped(records[piece].name)) {
            misfits.append(records[piece].name).append(" ");
        }
        names.push_back(ToUpperAscii(records[piece].name));
    }
    CHECK_MESSAGE(misfits.empty(), misfits);
    std::ranges::sort(names);
    CHECK(std::ranges::adjacent_find(names) == names.end());
    CHECK_FALSE(IsExtraPieceType(0x13EE));
}

TEST_CASE("every extra piece fits the grid and has a cell to rest on") {
    for (const PieceRecord& record : ExtraPieceRecords()) {
        CAPTURE(record.name);
        const PieceShape& shape = record.shape;
        CHECK(shape.cells.size() == static_cast<std::size_t>(shape.size.width * shape.size.height));
        CHECK(FitsSomeWay(shape));
        CHECK(std::ranges::any_of(shape.cells, &ShapeCell::RestsOnBelow));
    }
}

TEST_CASE("an extra piece is found by name in any case and has its one LDraw part") {
    CHECK(IsExtraPiece("x3024"));
    CHECK_FALSE(IsExtraPiece("L300100"));
    REQUIRE(LdrawPartsOf("x3024").size() == 1);
    CHECK(LdrawPartsOf("X3024").front().file == "3024.dat");
    CHECK(LdrawPartsOf("L300100").front().file == "3001.dat");
}

TEST_CASE("an extra piece's cells read across then down, column by column in the shape") {
    const std::vector<PieceRecord> records = ExtraPieceRecords();
    const auto corner = std::ranges::find(records, std::string("X2357"), &PieceRecord::name);
    REQUIRE(corner != records.end());
    CHECK(corner->shape.size == CellSize{.width = 2, .height = 2});
    CHECK(corner->shape.GetCell(1, 1, 0).IsEmpty());
    CHECK(corner->shape.GetCell(1, 0, 0) == ShapeCell{.top = 0x83, .bottom = 0x80});
    CHECK(corner->shape.GetCell(0, 1, 0) == ShapeCell{.top = 0x83, .bottom = 0x80});
}

}
}
