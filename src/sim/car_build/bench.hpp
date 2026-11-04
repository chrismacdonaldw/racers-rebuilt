#pragma once

#include <cstddef>
#include <cstdint>
#include <expected>
#include <variant>
#include <vector>

#include "sim/car_build/built_car.hpp"
#include "sim/car_build/car_grid.hpp"
#include "sim/car_build/loose_bricks.hpp"

namespace rr::sim {

inline constexpr std::size_t kBenchHistorySteps = 256;

enum class BenchArea : std::uint8_t { Car, Ground };

struct Bench {
    BuiltCar car;
    LooseBricks ground;
};

/// `piece.placement` is where it goes in `area`.
struct PutPiece {
    BenchArea area = BenchArea::Car;
    CarPiece piece;
};

struct LiftPiece {
    BenchArea area = BenchArea::Car;
    std::size_t index = 0;
};

struct ShiftPiece {
    BenchArea from = BenchArea::Car;
    std::size_t index = 0;
    BenchArea to = BenchArea::Car;
    PiecePlacement placement;
};

struct PaintPiece {
    BenchArea area = BenchArea::Car;
    std::size_t index = 0;
    std::uint8_t colour = 0;
};

using BenchEdit = std::variant<PutPiece, LiftPiece, ShiftPiece, PaintPiece>;

/// Undo and redo for a bench that changes only through `Do`, `Undo` and `Redo`.
class BenchHistory {
public:
    /// A refused edit leaves the bench as it was. A new edit forgets what was undone.
    std::expected<void, PlacementFailure> Do(Bench& bench, const BenchEdit& edit);

    bool Undo(Bench& bench);
    bool Redo(Bench& bench);

    [[nodiscard]] bool CanUndo() const { return !done_.empty(); }
    [[nodiscard]] bool CanRedo() const { return !undone_.empty(); }

private:
    struct Snapshot {
        BuiltCar car;
        std::vector<CarPiece> ground;
    };

    [[nodiscard]] static Snapshot Take(const Bench& bench);
    static void Restore(Bench& bench, const Snapshot& snapshot);

    std::vector<Snapshot> done_;
    std::vector<Snapshot> undone_;
};

}
