#include "sim/car_build/loose_bricks.hpp"

#include <cstddef>
#include <vector>

namespace rr::sim {
namespace {

bool CoversCar(const assets::PieceRecord& piece, PiecePlacement placement) {
    const assets::CellSize footprint = piece.shape.Footprint(placement.rotation);
    return placement.x < kCarOnGround.x + kCarGridWidth &&
           kCarOnGround.x < placement.x + footprint.width &&
           placement.y < kCarOnGround.y + kCarGridDepth &&
           kCarOnGround.y < placement.y + footprint.height;
}

bool IsOnGround(const CarPiece& piece) {
    const assets::CellSize footprint = piece.piece->shape.Footprint(piece.placement.rotation);
    return piece.placement.x >= 0 && piece.placement.y >= 0 &&
           piece.placement.x + footprint.width <= kGroundSize.width &&
           piece.placement.y + footprint.height <= kGroundSize.height;
}

}

LooseBricks::LooseBricks(std::span<const CarPiece> pieces) {
    for (const CarPiece& piece : pieces) {
        if (IsOnGround(piece)) {
            pieces_.push_back(piece);
        }
    }
    Settle();
}

std::expected<int, PlacementFailure> LooseBricks::Test(const assets::PieceRecord& piece,
                                                       PiecePlacement placement) const {
    const std::expected<int, PlacementFailure> height = grid_.RestingHeight(piece, placement, true);
    if (!height) {
        return height;
    }
    if (CoversCar(piece, placement) || grid_.Collides(piece, placement, *height)) {
        return std::unexpected(PlacementFailure::Collision);
    }
    if (*height + TopOf(piece) > kMaxCarHeight) {
        return std::unexpected(PlacementFailure::TooHigh);
    }
    if (pieces_.size() >= kMaxLoosePieces) {
        return std::unexpected(PlacementFailure::TooManyPieces);
    }
    return height;
}

std::expected<int, PlacementFailure> LooseBricks::Put(const CarPiece& piece) {
    if (piece.piece->IsChassis()) {
        return std::unexpected(PlacementFailure::Chassis);
    }
    CarPiece laid = piece;
    laid.placement.rotation &= 3U;
    const std::expected<int, PlacementFailure> height = Test(*laid.piece, laid.placement);
    if (height) {
        laid.height = *height;
        grid_.Stamp(*laid.piece, laid.placement, laid.height, static_cast<int>(pieces_.size()));
        pieces_.push_back(laid);
    }
    return height;
}

std::expected<CarPiece, PlacementFailure> LooseBricks::Take(std::size_t index) {
    if (index >= pieces_.size()) {
        return std::unexpected(PlacementFailure::NoPiece);
    }
    const CarPiece taken = pieces_[index];
    pieces_.erase(pieces_.begin() + static_cast<std::ptrdiff_t>(index));
    Settle();
    return taken;
}

std::expected<int, PlacementFailure> LooseBricks::Move(std::size_t index,
                                                       PiecePlacement placement) {
    if (index >= pieces_.size()) {
        return std::unexpected(PlacementFailure::NoPiece);
    }
    const std::vector<CarPiece> before = pieces_;
    CarPiece moved = *Take(index);
    moved.placement = placement;
    const std::expected<int, PlacementFailure> laid = Put(moved);
    if (!laid) {
        *this = LooseBricks(before);
    }
    return laid;
}

bool LooseBricks::Recolour(std::size_t index, std::uint8_t colour) {
    if (index >= pieces_.size()) {
        return false;
    }
    pieces_[index].colour = colour;
    return true;
}

std::optional<std::size_t> LooseBricks::PieceAt(int cellX, int cellY, int plate) const {
    for (std::size_t index = pieces_.size(); index > 0; --index) {
        if (Fills(pieces_[index - 1], cellX, cellY, plate)) {
            return index - 1;
        }
    }
    return std::nullopt;
}

void LooseBricks::Settle() {
    grid_.Clear();
    for (std::size_t index = 0; index < pieces_.size(); ++index) {
        CarPiece& piece = pieces_[index];
        const std::expected<int, PlacementFailure> rest =
            grid_.RestingHeight(*piece.piece, piece.placement, true);
        if (rest && !grid_.Collides(*piece.piece, piece.placement, *rest)) {
            piece.height = *rest;
        }
        grid_.Stamp(*piece.piece, piece.placement, piece.height, static_cast<int>(index));
    }
}

}
