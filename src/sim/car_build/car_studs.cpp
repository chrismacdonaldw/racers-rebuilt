#include "sim/car_build/car_studs.hpp"

#include <cstddef>
#include <span>

namespace rr::sim {
namespace {

CarStud StudOn(std::span<const CarPiece> pieces, const GridCell& cell, int x, int y) {
    const CarPiece& top = pieces[static_cast<std::size_t>(cell.piece)];
    const PiecePlacement at = top.placement;
    const int raised =
        top.piece->IsChassis()
            ? top.piece->shape.GetCell(x - at.x, y - at.y, at.rotation).RaisedBottomHeight()
            : 0;
    return {.x = x,
            .y = y,
            .height = cell.height,
            .colour = top.colour,
            .raisedBottom = raised,
            .topPiece = static_cast<std::size_t>(cell.piece)};
}

void AddCoveredStuds(const CarGrid& grid, std::span<const CarPiece> pieces, const CarPiece& piece,
                     std::vector<CarStud>& studs) {
    const PiecePlacement at = piece.placement;
    const assets::CellSize footprint = piece.piece->shape.Footprint(at.rotation);
    for (int x = 0; x < footprint.width; ++x) {
        for (int y = 0; y < footprint.height; ++y) {
            const assets::ShapeCell cell = piece.piece->shape.GetCell(x, y, at.rotation);
            const GridCell& below = grid.At(at.x + x, at.y + y);
            if (cell.IsEmpty() || !below.isOccupied) {
                continue;
            }
            const int flush = piece.height + cell.ClampedBottomHeight();
            if (below.height != flush || cell.SitsOverStuds()) {
                studs.push_back(StudOn(pieces, below, at.x + x, at.y + y));
            }
        }
    }
}

}

std::vector<CarStud> CarStuds(const BuiltCar& car) {
    const std::span<const CarPiece> pieces = car.Pieces();
    std::vector<CarStud> studs;
    for (int x = 0; x < kCarGridWidth; ++x) {
        for (int y = 0; y < kCarGridDepth; ++y) {
            const GridCell& cell = car.Grid().At(x, y);
            if (cell.isOccupied) {
                studs.push_back(StudOn(pieces, cell, x, y));
            }
        }
    }
    CarGrid replay;
    for (std::size_t index = 0; index < pieces.size(); ++index) {
        const CarPiece& piece = pieces[index];
        AddCoveredStuds(replay, pieces, piece, studs);
        replay.Stamp(*piece.piece, piece.placement, piece.height, static_cast<int>(index));
    }
    return studs;
}

}
