#pragma once

#include "assets/piece_library.hpp"
#include "sim/car_build/car_grid.hpp"

namespace rr::sim {

/// Pins the corner nearest the grid's edges, so a new piece keeps to the side the old one was on.
class PieceCursor {
public:
    /// The first piece held starts centred on the grid.
    void Hold(const assets::PieceRecord& piece);

    void Rotate();

    bool MoveAcross(int cells);
    bool MoveDeep(int cells);

    /// Kept on the grid, as an undone piece is.
    void PutAt(const assets::PieceRecord& piece, PiecePlacement placement);

    [[nodiscard]] const assets::PieceRecord* Piece() const { return piece_; }
    [[nodiscard]] PiecePlacement Placement() const;

private:
    [[nodiscard]] assets::CellSize Footprint() const;
    [[nodiscard]] assets::CellSize FootprintAt(unsigned rotation) const;
    [[nodiscard]] bool Fits(unsigned rotation) const;
    void ToGrid();
    void FromGrid();
    void PinNearestCorner();
    void ClampToGrid();
    bool Shift(int& grid, int cells, int extent, int limit);

    const assets::PieceRecord* piece_ = nullptr;
    int width_ = 0;
    int depth_ = 0;
    unsigned rotation_ = 0;
    unsigned anchor_ = 0;
    int x_ = 0;
    int y_ = 0;
    int gridX_ = 0;
    int gridY_ = 0;
};

}
