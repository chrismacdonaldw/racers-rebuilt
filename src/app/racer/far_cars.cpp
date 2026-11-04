#include "app/racer/far_cars.hpp"

#include "app/racer/slot_draws.hpp"

namespace rr::app {

void AddGridCars(const RacerGrid& racers, std::span<const sim::EntitySnapshot> poses,
                 std::size_t firstDraw, FarModels& out) {
    for (std::size_t slot = 0; slot < racers.Count() && slot < poses.size(); ++slot) {
        out.Add(FarKind::Car, {.position = poses[slot].transform.position,
                               .draws = SlotDrawSpan(racers, slot, firstDraw),
                               .rangeSquared = kCarRangeSquared});
    }
}

}
