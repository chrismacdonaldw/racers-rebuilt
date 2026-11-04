#include "app/racer/slot_draws.hpp"

#include <algorithm>

namespace rr::app {

DrawSpan SlotDrawSpan(const RacerGrid& racers, std::size_t slot, std::size_t firstDraw) {
    std::size_t first = firstDraw;
    for (std::size_t before = 0; before < slot; ++before) {
        first += racers.SlotDraws(before).size();
    }
    return {.first = first, .count = racers.SlotDraws(slot).size()};
}

std::span<const render::MeshDraw> WithoutDraws(std::span<const render::MeshDraw> meshes,
                                               DrawSpan hidden,
                                               std::vector<render::MeshDraw>& copy) {
    copy.assign(meshes.begin(), meshes.end());
    const std::size_t first = std::min(hidden.first, copy.size());
    const std::size_t end = std::min(hidden.first + hidden.count, copy.size());
    for (std::size_t draw = first; draw < end; ++draw) {
        copy[draw].indexCount = 0;
    }
    return copy;
}

}
