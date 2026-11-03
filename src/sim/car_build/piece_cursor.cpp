#include "sim/car_build/piece_cursor.hpp"

#include <algorithm>
#include <cstdlib>

namespace rr::sim {
namespace {

constexpr unsigned kRotations = 4;
constexpr unsigned kFarDeep = 1;
constexpr unsigned kFarAcross = 2;

}

void PieceCursor::Hold(const assets::PieceRecord& piece) {
    width_ = piece.shape.size.width;
    depth_ = piece.shape.size.height;
    if (piece_ == nullptr) {
        anchor_ = 0;
        rotation_ = 0;
        gridX_ = (kCarGridWidth - width_) >> 1;
        gridY_ = (kCarGridDepth - depth_) >> 1;
        if (gridX_ < 0 || gridY_ < 0) {
            rotation_ = 1;
            gridX_ = (kCarGridWidth - depth_) >> 1;
            gridY_ = (kCarGridDepth - width_) >> 1;
        }
    } else {
        if (!Fits(rotation_)) {
            ++rotation_;
        }
        rotation_ %= kRotations;
        ToGrid();
        const assets::CellSize footprint = Footprint();
        gridX_ = std::min(gridX_, kCarGridWidth - footprint.width);
        gridY_ = std::min(gridY_, kCarGridDepth - footprint.height);
    }
    gridX_ = std::max(gridX_, 0);
    gridY_ = std::max(gridY_, 0);
    piece_ = &piece;
    FromGrid();
    PinNearestCorner();
}

void PieceCursor::Rotate() {
    if (!Fits(rotation_ + 1)) {
        ++rotation_;
    }
    rotation_ = (rotation_ + 1) % kRotations;
    ClampToGrid();
}

bool PieceCursor::MoveAcross(int cells) {
    return Shift(gridX_, cells, Footprint().width, kCarGridWidth);
}

bool PieceCursor::MoveDeep(int cells) {
    return Shift(gridY_, cells, Footprint().height, kCarGridDepth);
}

void PieceCursor::PutAt(const assets::PieceRecord& piece, PiecePlacement placement) {
    x_ = placement.x;
    y_ = placement.y;
    rotation_ = placement.rotation % kRotations;
    anchor_ = 0;
    piece_ = &piece;
    Hold(piece);
}

PiecePlacement PieceCursor::Placement() const {
    return {.x = gridX_, .y = gridY_, .rotation = rotation_};
}

assets::CellSize PieceCursor::Footprint() const {
    return FootprintAt(rotation_);
}

assets::CellSize PieceCursor::FootprintAt(unsigned rotation) const {
    return (rotation & 1U) != 0 ? assets::CellSize{.width = depth_, .height = width_}
                                : assets::CellSize{.width = width_, .height = depth_};
}

bool PieceCursor::Fits(unsigned rotation) const {
    const assets::CellSize footprint = FootprintAt(rotation);
    return footprint.width <= kCarGridWidth && footprint.height <= kCarGridDepth;
}

void PieceCursor::ToGrid() {
    const assets::CellSize footprint = Footprint();
    gridX_ = x_ - ((anchor_ & kFarAcross) != 0 ? footprint.width : 0);
    gridY_ = y_ - ((anchor_ & kFarDeep) != 0 ? footprint.height : 0);
}

void PieceCursor::FromGrid() {
    const assets::CellSize footprint = Footprint();
    x_ = gridX_ + ((anchor_ & kFarAcross) != 0 ? footprint.width : 0);
    y_ = gridY_ + ((anchor_ & kFarDeep) != 0 ? footprint.height : 0);
}

void PieceCursor::PinNearestCorner() {
    const unsigned across = x_ >= kCarGridWidth / 2 ? kFarAcross : 0;
    const unsigned deep = y_ >= kCarGridDepth / 2 ? kFarDeep : 0;
    ToGrid();
    anchor_ = across | deep;
    FromGrid();
}

void PieceCursor::ClampToGrid() {
    ToGrid();
    const assets::CellSize footprint = Footprint();
    gridX_ = std::min(std::max(gridX_, 0), kCarGridWidth - footprint.width);
    gridY_ = std::min(std::max(gridY_, 0), kCarGridDepth - footprint.height);
    FromGrid();
}

bool PieceCursor::Shift(int& grid, int cells, int extent, int limit) {
    const PiecePlacement before = Placement();
    const int step = cells > 0 ? 1 : -1;
    for (int left = std::abs(cells); left > 0; --left) {
        const bool canStep = step > 0 ? grid + extent < limit : grid > 0;
        if (canStep) {
            grid += step;
            FromGrid();
            PinNearestCorner();
        }
    }
    return Placement() != before;
}

}
