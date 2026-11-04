#include "app/menu/car_builder.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <expected>
#include <optional>
#include <utility>
#include <vector>

#include "app/menu/car_edit.hpp"

namespace rr::app {
namespace {

constexpr std::array<int, kBuildViews * kBuildDirections> kAcrossSteps = {
    -1, -1, 0,  1, 1,  1,  0,  -1, -1, -1, -1, 0,  1,  1,  1,  0,  0, -1, -1, -1, 0, 1,
    1,  1,  1,  0, -1, -1, -1, 0,  1,  1,  1,  1,  0,  -1, -1, -1, 0, 1,  1,  1,  1, 0,
    -1, -1, -1, 0, 0,  1,  1,  1,  0,  -1, -1, -1, -1, 0,  1,  1,  1, 0,  -1, -1,
};

constexpr std::array<int, kBuildViews * kBuildDirections> kDeepSteps = {
    0,  1,  1,  1,  0,  -1, -1, -1, -1, 0,  1, 1,  1,  0,  -1, -1, -1, -1, 0,  1, 1,  1,
    0,  -1, -1, -1, -1, 0,  1,  1,  1,  0,  0, -1, -1, -1, 0,  1,  1,  1,  1,  0, -1, -1,
    -1, 0,  1,  1,  1,  1,  0,  -1, -1, -1, 0, 1,  1,  1,  1,  0,  -1, -1, -1, 0,
};

std::size_t FindChoice(const assets::PieceCatalog& catalog, std::size_t set, std::uint16_t type,
                       std::uint8_t colour) {
    const std::vector<assets::PartChoice>& choices = catalog.partSets[set].choices;
    for (std::size_t i = 0; i < choices.size(); ++i) {
        if (catalog.library.pieces[choices[i].piece].type == type && choices[i].colour == colour) {
            return i;
        }
    }
    return 0;
}

template <typename Value>
PlaceResult ResultOf(const std::expected<Value, sim::PlacementFailure>& placed) {
    if (placed) {
        return PlaceResult::Placed;
    }
    return placed.error() == sim::PlacementFailure::TooManyPieces ? PlaceResult::TooManyPieces
                                                                  : PlaceResult::Refused;
}

}

CarBuilder::CarBuilder(const assets::PieceCatalog& catalog, sim::BuiltCar car, std::size_t set)
    : catalog_(&catalog), car_(std::move(car)) {
    SelectSet(set);
}

void CarBuilder::SelectSet(std::size_t set) {
    if (set >= catalog_->partSets.size()) {
        return;
    }
    set_ = set;
    Choose(0);
}

void CarBuilder::Choose(std::size_t choice) {
    const std::vector<assets::PartChoice>& choices = catalog_->partSets[set_].choices;
    if (choice >= choices.size()) {
        return;
    }
    choice_ = choice;
    colour_ = choices[choice].colour;
    cursor_.Hold(catalog_->library.pieces[choices[choice].piece]);
}

void CarBuilder::Pick(std::size_t set, std::size_t choice, std::uint8_t colour) {
    if (set >= catalog_->partSets.size()) {
        return;
    }
    set_ = set;
    Choose(choice);
    colour_ = colour;
}

void CarBuilder::PutAt(sim::PiecePlacement placement) {
    if (cursor_.Piece() != nullptr) {
        cursor_.PutAt(*cursor_.Piece(), placement);
    }
}

bool CarBuilder::Move(std::size_t direction, std::size_t view) {
    const std::size_t step =
        ((view % kBuildViews) * kBuildDirections) + (direction % kBuildDirections);
    const int across = kAcrossSteps[step];
    const int deep = kDeepSteps[step];
    const bool isDiagonal = (direction & 1U) != 0;
    if (across != 0 && deep != 0 && !isDiagonal) {
        if (cursor_.MoveAcross(across) && cursor_.MoveDeep(deep)) {
            return true;
        }
        cursor_.MoveAcross(-across);
        return false;
    }
    const bool movedAcross = cursor_.MoveAcross(across);
    const bool movedDeep = cursor_.MoveDeep(deep);
    return movedAcross || movedDeep;
}

void CarBuilder::Rotate() {
    if (cursor_.Piece() != nullptr) {
        cursor_.Rotate();
    }
}

PlaceResult CarBuilder::Place() {
    if (cursor_.Piece() == nullptr) {
        return PlaceResult::Refused;
    }
    return ResultOf(
        car_.Place(*cursor_.Piece(), cursor_.Placement(), colour_, PartTypeOf(*catalog_, set_)));
}

PlaceResult CarBuilder::Check() const {
    if (cursor_.Piece() == nullptr) {
        return PlaceResult::Refused;
    }
    return ResultOf(car_.Test(*cursor_.Piece(), cursor_.Placement()));
}

int CarBuilder::RestPlates() const {
    const assets::PieceRecord* piece = cursor_.Piece();
    if (piece == nullptr) {
        return 0;
    }
    const sim::PiecePlacement at = cursor_.Placement();
    const assets::CellSize size = piece->shape.Footprint(at.rotation);
    int height = 0;
    for (int y = 0; y < size.height; ++y) {
        for (int x = 0; x < size.width; ++x) {
            if (piece->shape.GetCell(x, y, at.rotation).Occupied()) {
                height = std::max(height, car_.Grid().At(at.x + x, at.y + y).height);
            }
        }
    }
    return height;
}

bool CarBuilder::Undo() {
    const std::optional<sim::CarPiece> undone = car_.Undo();
    if (!undone) {
        return false;
    }
    if (const std::optional<std::size_t> set = PartSetOfType(*catalog_, undone->partType)) {
        set_ = *set;
        choice_ = FindChoice(*catalog_, set_, undone->piece->type, undone->colour);
    }
    colour_ = undone->colour;
    cursor_.PutAt(*undone->piece, undone->placement);
    return true;
}

}
