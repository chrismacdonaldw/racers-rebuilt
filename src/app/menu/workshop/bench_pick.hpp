#pragma once

#include <cstddef>
#include <optional>

#include "app/menu/grid_pick.hpp"
#include "assets/piece_library.hpp"
#include "render/camera.hpp"
#include "render/render_view.hpp"
#include "sim/car_build/bench.hpp"

namespace rr::app {

/// The corners are where each grid's first cell sits in game space, at its zero height.
struct BenchView {
    render::Camera camera;
    render::Rect view;
    Vector3 carCorner;
    Vector3 groundCorner;
};

struct BenchCell {
    sim::BenchArea area = sim::BenchArea::Car;
    GridCellPick cell;

    friend bool operator==(const BenchCell&, const BenchCell&) = default;
};

struct BenchPiece {
    sim::BenchArea area = sim::BenchArea::Car;
    std::size_t index = 0;

    friend bool operator==(const BenchPiece&, const BenchPiece&) = default;
};

/// The car or ground cell whose top the ray through canvas point (`x`, `y`) meets first.
[[nodiscard]] std::optional<BenchCell> PickBenchCell(const BenchView& at, const sim::Bench& bench,
                                                     float x, float y);

/// The piece on top of the cell `PickBenchCell` finds.
[[nodiscard]] std::optional<BenchPiece> PickBenchPiece(const BenchView& at, const sim::Bench& bench,
                                                       float x, float y);

/// Where a piece of `size` centred on `cell` goes, kept on its area's grid.
[[nodiscard]] sim::PiecePlacement PlaceOn(const BenchCell& cell, assets::CellSize size,
                                          unsigned rotation);

[[nodiscard]] const sim::CarPiece* PieceOf(const sim::Bench& bench, const BenchPiece& piece);

}
