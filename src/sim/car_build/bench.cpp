#include "sim/car_build/bench.hpp"

#include <utility>

namespace rr::sim {
namespace {

using Done = std::expected<void, PlacementFailure>;

Done Succeeded(const auto& result) {
    if (!result) {
        return std::unexpected(result.error());
    }
    return {};
}

Done Put(Bench& bench, BenchArea area, const CarPiece& piece) {
    if (area == BenchArea::Ground) {
        return Succeeded(bench.ground.Put(piece));
    }
    if (piece.piece->IsChassis()) {
        return std::unexpected(PlacementFailure::Chassis);
    }
    return Succeeded(bench.car.Place(*piece.piece, piece.placement, piece.colour, piece.partType));
}

std::expected<CarPiece, PlacementFailure> Lift(Bench& bench, BenchArea area, std::size_t index) {
    return area == BenchArea::Ground ? bench.ground.Take(index) : bench.car.Remove(index);
}

Done Apply(Bench& bench, const PutPiece& put) {
    return Put(bench, put.area, put.piece);
}

Done Apply(Bench& bench, const LiftPiece& lift) {
    return Succeeded(Lift(bench, lift.area, lift.index));
}

Done Apply(Bench& bench, const ShiftPiece& shift) {
    if (shift.from == shift.to) {
        return shift.to == BenchArea::Ground
                   ? Succeeded(bench.ground.Move(shift.index, shift.placement))
                   : Succeeded(bench.car.Move(shift.index, shift.placement));
    }
    std::expected<CarPiece, PlacementFailure> lifted = Lift(bench, shift.from, shift.index);
    if (!lifted) {
        return std::unexpected(lifted.error());
    }
    lifted->placement = shift.placement;
    return Put(bench, shift.to, *lifted);
}

Done Apply(Bench& bench, const PaintPiece& paint) {
    const bool isPainted = paint.area == BenchArea::Ground
                               ? bench.ground.Recolour(paint.index, paint.colour)
                               : bench.car.Recolour(paint.index, paint.colour);
    if (!isPainted) {
        return std::unexpected(PlacementFailure::NoPiece);
    }
    return {};
}

}

Done BenchHistory::Do(Bench& bench, const BenchEdit& edit) {
    Snapshot before = Take(bench);
    const Done done = std::visit([&](const auto& change) { return Apply(bench, change); }, edit);
    if (!done) {
        Restore(bench, before);
        return done;
    }
    if (done_.size() == kBenchHistorySteps) {
        done_.erase(done_.begin());
    }
    done_.push_back(std::move(before));
    undone_.clear();
    return done;
}

bool BenchHistory::Undo(Bench& bench) {
    if (done_.empty()) {
        return false;
    }
    undone_.push_back(Take(bench));
    Restore(bench, done_.back());
    done_.pop_back();
    return true;
}

bool BenchHistory::Redo(Bench& bench) {
    if (undone_.empty()) {
        return false;
    }
    done_.push_back(Take(bench));
    Restore(bench, undone_.back());
    undone_.pop_back();
    return true;
}

BenchHistory::Snapshot BenchHistory::Take(const Bench& bench) {
    return {.car = bench.car,
            .ground = {bench.ground.Pieces().begin(), bench.ground.Pieces().end()}};
}

void BenchHistory::Restore(Bench& bench, const Snapshot& snapshot) {
    bench.car = snapshot.car;
    bench.ground = LooseBricks(snapshot.ground);
}

}
