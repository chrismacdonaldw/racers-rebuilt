#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <vector>

#include "assets/piece_library.hpp"

namespace rr::sim {

inline constexpr int kCarGridWidth = assets::kBuildGridSize.width;
inline constexpr int kCarGridDepth = assets::kBuildGridSize.height;

struct PiecePlacement {
    int x = 0;
    int y = 0;
    unsigned rotation = 0;

    friend bool operator==(const PiecePlacement&, const PiecePlacement&) = default;
};

/// `piece` indexes the build's pieces, -1 for none. `height` is in plates.
struct GridCell {
    int piece = -1;
    int height = 0;
    bool isOccupied = false;
};

enum class PlacementFailure : std::uint8_t {
    NoSupport,
    OffLeftOrTop,
    PastRight,
    PastBottom,
    Collision,
    TooHigh,
    TooManyPieces,
    NoPiece,
    Chassis,
    HoldsPieces,
};

/// Keeps each cell's top height and the piece on top. The car builder's 10×6 by default.
class CarGrid {
public:
    CarGrid() : CarGrid(assets::kBuildGridSize) {}
    explicit CarGrid(assets::CellSize size);

    void Clear();

    [[nodiscard]] assets::CellSize Size() const { return size_; }

    [[nodiscard]] const GridCell& At(int x, int y) const;

    /// In plates. `allowsEmpty` also rests the piece on bare cells, as the chassis does.
    [[nodiscard]] std::expected<int, PlacementFailure> RestingHeight(
        const assets::PieceRecord& piece, PiecePlacement placement, bool allowsEmpty) const;

    [[nodiscard]] bool Collides(const assets::PieceRecord& piece, PiecePlacement placement,
                                int height) const;

    void Stamp(const assets::PieceRecord& piece, PiecePlacement placement, int height, int index);

private:
    [[nodiscard]] std::size_t CellIndex(int x, int y) const;
    [[nodiscard]] GridCell& Cell(int x, int y);

    assets::CellSize size_;
    std::vector<GridCell> cells_;
};

}
