#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <variant>
#include <vector>

#include "sim/car_build/built_car.hpp"

namespace rr::sim {

inline constexpr std::size_t kBuildHistorySteps = 256;

struct PlacePiece {
    CarPiece piece;
};

struct RemovePiece {
    std::size_t index = 0;
};

struct MovePiece {
    std::size_t index = 0;
    PiecePlacement placement;
};

struct RecolourPiece {
    std::size_t index = 0;
    std::uint8_t colour = 0;
};

using BuildEdit = std::variant<PlacePiece, RemovePiece, MovePiece, RecolourPiece>;

/// Undo and redo for a car that changes only through `Do`, `Undo` and `Redo`.
class BuildHistory {
public:
    /// Refuses a chassis, which never comes off. A new edit forgets what was undone.
    std::expected<void, PlacementFailure> Do(BuiltCar& car, const BuildEdit& edit);

    bool Undo(BuiltCar& car);
    bool Redo(BuiltCar& car);

    [[nodiscard]] bool CanUndo() const { return !done_.empty(); }
    [[nodiscard]] bool CanRedo() const { return !undone_.empty(); }

private:
    struct Step {
        BuildEdit edit;
        CarPiece before;
    };

    std::vector<Step> done_;
    std::vector<Step> undone_;
};

}
