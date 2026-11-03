#include "app/racer/far_cars.hpp"

namespace rr::app {

void AddGridCars(const RacerGrid& racers, std::span<const sim::EntitySnapshot> poses,
                 std::size_t firstDraw, FarModels& out) {
    std::size_t first = firstDraw;
    for (std::size_t slot = 0; slot < racers.Count() && slot < poses.size(); ++slot) {
        const std::size_t count = racers.SlotDraws(slot).size();
        out.Add(FarKind::Car, {.position = poses[slot].transform.position,
                               .draws = {.first = first, .count = count},
                               .rangeSquared = kCarRangeSquared});
        first += count;
    }
}

}
