#include "sim/car_build/car_grid.hpp"

#include <algorithm>
#include <cstddef>

#include "core/assert.hpp"

namespace rr::sim {

CarGrid::CarGrid(assets::CellSize size)
    : size_(size), cells_(static_cast<std::size_t>(size.width * size.height)) {}

void CarGrid::Clear() {
    std::ranges::fill(cells_, GridCell{});
}

std::size_t CarGrid::CellIndex(int x, int y) const {
    RR_ASSERT(x >= 0 && x < size_.width && y >= 0 && y < size_.height);
    return (static_cast<std::size_t>(x) * static_cast<std::size_t>(size_.height)) +
           static_cast<std::size_t>(y);
}

const GridCell& CarGrid::At(int x, int y) const {
    return cells_.at(CellIndex(x, y));
}

int CarGrid::Tallest() const {
    const auto tallest = std::ranges::max_element(cells_, {}, &GridCell::height);
    return tallest == cells_.end() ? 0 : tallest->height;
}

GridCell& CarGrid::Cell(int x, int y) {
    return cells_.at(CellIndex(x, y));
}

std::expected<int, PlacementFailure> CarGrid::RestingHeight(const assets::PieceRecord& piece,
                                                            PiecePlacement placement,
                                                            bool allowsEmpty) const {
    if (placement.x < 0 || placement.y < 0) {
        return std::unexpected(PlacementFailure::OffLeftOrTop);
    }
    const assets::CellSize footprint = piece.shape.Footprint(placement.rotation);
    if (placement.x + footprint.width > size_.width) {
        return std::unexpected(PlacementFailure::PastRight);
    }
    if (placement.y + footprint.height > size_.height) {
        return std::unexpected(PlacementFailure::PastBottom);
    }
    int height = -1;
    for (int x = 0; x < footprint.width; ++x) {
        for (int y = 0; y < footprint.height; ++y) {
            const GridCell& below = At(placement.x + x, placement.y + y);
            const assets::ShapeCell cell = piece.shape.GetCell(x, y, placement.rotation);
            if (below.isOccupied && cell.RestsOnBelow()) {
                height = std::max(height, below.height - cell.ClampedBottomHeight());
            } else if (!below.isOccupied && allowsEmpty && below.height == 0) {
                height = std::max(height, -cell.ClampedBottomHeight());
            }
        }
    }
    if (height < 0) {
        return std::unexpected(PlacementFailure::NoSupport);
    }
    return height;
}

bool CarGrid::Collides(const assets::PieceRecord& piece, PiecePlacement placement,
                       int height) const {
    const assets::CellSize footprint = piece.shape.Footprint(placement.rotation);
    for (int x = 0; x < footprint.width; ++x) {
        for (int y = 0; y < footprint.height; ++y) {
            const assets::ShapeCell cell = piece.shape.GetCell(x, y, placement.rotation);
            if (cell.IsEmpty()) {
                continue;
            }
            const GridCell& below = At(placement.x + x, placement.y + y);
            const int overlap = below.height - height;
            const int bottom = cell.ClampedBottomHeight();
            if (overlap > bottom ||
                (overlap == bottom && below.isOccupied && !cell.CanSitLevel())) {
                return true;
            }
        }
    }
    return false;
}

void CarGrid::Stamp(const assets::PieceRecord& piece, PiecePlacement placement, int height,
                    int index) {
    const assets::CellSize footprint = piece.shape.Footprint(placement.rotation);
    for (int x = 0; x < footprint.width; ++x) {
        for (int y = 0; y < footprint.height; ++y) {
            const assets::ShapeCell cell = piece.shape.GetCell(x, y, placement.rotation);
            if (cell.IsEmpty()) {
                continue;
            }
            Cell(placement.x + x, placement.y + y) = {
                .piece = index, .height = height + cell.TopHeight(), .isOccupied = cell.Occupied()};
        }
    }
}

}
