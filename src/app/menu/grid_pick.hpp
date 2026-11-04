#pragma once

#include <optional>

#include "core/transform.hpp"
#include "core/vector3.hpp"
#include "render/camera.hpp"
#include "render/render_view.hpp"
#include "sim/car_build/car_grid.hpp"

namespace rr::app {

inline constexpr float kPlateHeight = 0.4F;

/// `car` places the car in game space, and `corner` is the grid's corner in the car.
struct GridView {
    render::Camera camera;
    render::Rect view;
    Transform car;
    Vector3 corner;
};

struct GridCellPick {
    int x = 0;
    int y = 0;

    friend bool operator==(const GridCellPick&, const GridCellPick&) = default;
};

/// The cell whose top the ray through canvas point (`x`, `y`) meets first.
[[nodiscard]] std::optional<GridCellPick> PickGridCell(const GridView& at, const sim::CarGrid& grid,
                                                       float x, float y);

/// Clamped so the whole footprint stays on the grid.
[[nodiscard]] sim::PiecePlacement CentredOn(GridCellPick cell, assets::CellSize size,
                                            unsigned rotation);

}
