#include "app/menu/workshop/bench_pick.hpp"

#include <span>

#include "sim/car_build/loose_bricks.hpp"

namespace rr::app {
namespace {

GridView ViewOf(const BenchView& at, const Vector3& corner) {
    return {.camera = at.camera, .view = at.view, .car = {}, .corner = corner};
}

const sim::CarGrid& GridOf(const sim::Bench& bench, sim::BenchArea area) {
    return area == sim::BenchArea::Car ? bench.car.Grid() : bench.ground.Grid();
}

}

std::optional<BenchCell> PickBenchCell(const BenchView& at, const sim::Bench& bench, float x,
                                       float y) {
    const std::optional<GridHit> car =
        HitGridCell(ViewOf(at, at.carCorner), bench.car.Grid(), x, y);
    const std::optional<GridHit> ground =
        HitGridCell(ViewOf(at, at.groundCorner), bench.ground.Grid(), x, y);
    if (car && (!ground || car->distance <= ground->distance)) {
        return BenchCell{.area = sim::BenchArea::Car, .cell = car->cell};
    }
    if (ground) {
        return BenchCell{.area = sim::BenchArea::Ground, .cell = ground->cell};
    }
    return std::nullopt;
}

std::optional<BenchPiece> PickBenchPiece(const BenchView& at, const sim::Bench& bench, float x,
                                         float y) {
    const std::optional<BenchCell> picked = PickBenchCell(at, bench, x, y);
    if (!picked) {
        return std::nullopt;
    }
    const int height = GridOf(bench, picked->area).At(picked->cell.x, picked->cell.y).height;
    if (height <= 0) {
        return std::nullopt;
    }
    const std::optional<std::size_t> index =
        picked->area == sim::BenchArea::Car
            ? bench.car.PieceAt(picked->cell.x, picked->cell.y, height - 1)
            : bench.ground.PieceAt(picked->cell.x, picked->cell.y, height - 1);
    return index.transform(
        [&](std::size_t found) { return BenchPiece{.area = picked->area, .index = found}; });
}

sim::PiecePlacement PlaceOn(const BenchCell& cell, assets::CellSize size, unsigned rotation) {
    const assets::CellSize area =
        cell.area == sim::BenchArea::Car ? assets::kBuildGridSize : sim::kGroundSize;
    return CentredOn(cell.cell, size, rotation, area);
}

const sim::CarPiece* PieceOf(const sim::Bench& bench, const BenchPiece& piece) {
    const std::span<const sim::CarPiece> pieces =
        piece.area == sim::BenchArea::Car ? bench.car.Pieces() : bench.ground.Pieces();
    return piece.index < pieces.size() ? &pieces[piece.index] : nullptr;
}

}
