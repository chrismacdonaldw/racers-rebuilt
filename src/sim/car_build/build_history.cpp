#include "sim/car_build/build_history.hpp"

#include <optional>
#include <type_traits>

namespace rr::sim {
namespace {

std::expected<void, PlacementFailure> Succeeded(const auto& result) {
    if (!result) {
        return std::unexpected(result.error());
    }
    return {};
}

std::expected<void, PlacementFailure> ApplyEdit(BuiltCar& car, const PlacePiece& place) {
    if (place.piece.piece->IsChassis()) {
        return std::unexpected(PlacementFailure::Chassis);
    }
    return Succeeded(car.Place(*place.piece.piece, place.piece.placement, place.piece.colour,
                               place.piece.partType));
}

std::expected<void, PlacementFailure> ApplyEdit(BuiltCar& car, const RemovePiece& remove) {
    return Succeeded(car.Remove(remove.index));
}

std::expected<void, PlacementFailure> ApplyEdit(BuiltCar& car, const MovePiece& move) {
    return Succeeded(car.Move(move.index, move.placement));
}

std::expected<void, PlacementFailure> ApplyEdit(BuiltCar& car, const RecolourPiece& recolour) {
    if (!car.Recolour(recolour.index, recolour.colour)) {
        return std::unexpected(PlacementFailure::NoPiece);
    }
    return {};
}

std::expected<void, PlacementFailure> Apply(BuiltCar& car, const BuildEdit& edit) {
    return std::visit([&](const auto& change) { return ApplyEdit(car, change); }, edit);
}

void RevertEdit(BuiltCar& car, [[maybe_unused]] const PlacePiece& place,
                [[maybe_unused]] const CarPiece& before) {
    static_cast<void>(car.Undo());
}

void RevertEdit(BuiltCar& car, const RemovePiece& remove, const CarPiece& before) {
    car.Insert(remove.index, before);
}

void RevertEdit(BuiltCar& car, const MovePiece& move, const CarPiece& before) {
    static_cast<void>(car.Undo());
    car.Insert(move.index, before);
}

void RevertEdit(BuiltCar& car, const RecolourPiece& recolour, const CarPiece& before) {
    car.Recolour(recolour.index, before.colour);
}

void Revert(BuiltCar& car, const BuildEdit& edit, const CarPiece& before) {
    std::visit([&](const auto& change) { RevertEdit(car, change, before); }, edit);
}

std::optional<std::size_t> EditedIndex(const BuildEdit& edit) {
    return std::visit(
        [](const auto& change) -> std::optional<std::size_t> {
            if constexpr (std::is_same_v<std::decay_t<decltype(change)>, PlacePiece>) {
                return std::nullopt;
            } else {
                return change.index;
            }
        },
        edit);
}

CarPiece PieceBefore(const BuiltCar& car, const BuildEdit& edit) {
    const std::optional<std::size_t> index = EditedIndex(edit);
    return index && *index < car.Pieces().size() ? car.Pieces()[*index] : CarPiece{};
}

}

std::expected<void, PlacementFailure> BuildHistory::Do(BuiltCar& car, const BuildEdit& edit) {
    const CarPiece before = PieceBefore(car, edit);
    const std::expected<void, PlacementFailure> applied = Apply(car, edit);
    if (!applied) {
        return applied;
    }
    undone_.clear();
    done_.push_back({.edit = edit, .before = before});
    if (done_.size() > kBuildHistorySteps) {
        done_.pop_front();
    }
    return applied;
}

bool BuildHistory::Undo(BuiltCar& car) {
    if (done_.empty()) {
        return false;
    }
    Revert(car, done_.back().edit, done_.back().before);
    undone_.push_back(done_.back());
    done_.pop_back();
    return true;
}

bool BuildHistory::Redo(BuiltCar& car) {
    if (undone_.empty() || !Apply(car, undone_.back().edit)) {
        return false;
    }
    done_.push_back(undone_.back());
    undone_.pop_back();
    return true;
}

}
