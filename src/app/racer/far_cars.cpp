#include "app/racer/far_cars.hpp"

#include <utility>

namespace rr::app {

std::vector<std::size_t> DrawsPerSlot(const RacerGrid& racers) {
    std::vector<std::size_t> counts;
    counts.reserve(racers.Count());
    for (std::size_t slot = 0; slot < racers.Count(); ++slot) {
        counts.push_back(racers.SlotDraws(slot).size());
    }
    return counts;
}

FarCars::FarCars(std::vector<std::size_t> drawsPerSlot) : drawsPerSlot_(std::move(drawsPerSlot)) {}

std::span<const render::MeshDraw> FarCars::Cull(std::size_t view, const Vector3& camera,
                                                std::span<const sim::EntitySnapshot> poses,
                                                std::span<const render::MeshDraw> meshes,
                                                std::size_t firstCar) {
    if (!isHidden_) {
        return meshes;
    }
    std::vector<render::MeshDraw>& culled = views_.at(view);
    culled.assign(meshes.begin(), meshes.end());
    std::size_t draw = firstCar;
    for (std::size_t slot = 0; slot < drawsPerSlot_.size() && slot < poses.size(); ++slot) {
        const Vector3 offset = poses[slot].transform.position - camera;
        const bool isFar = Dot(offset, offset) > kCarRangeSquared;
        for (std::size_t end = draw + drawsPerSlot_[slot]; draw < end && draw < culled.size();
             ++draw) {
            if (isFar) {
                culled[draw].indexCount = 0;
            }
        }
    }
    return culled;
}

}
