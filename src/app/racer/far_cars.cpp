#include "app/racer/far_cars.hpp"

namespace rr::app {

std::span<const render::MeshDraw> FarCars::Cull(std::size_t view, const Vector3& camera,
                                                std::vector<render::MeshDraw>& meshes) {
    if (!isHidden_) {
        return meshes;
    }
    if (view == 0) {
        Patch(camera, meshes);
        return meshes;
    }
    std::vector<render::MeshDraw>& copy = copies_.at(view);
    copy.assign(meshes.begin(), meshes.end());
    Patch(camera, copy);
    return copy;
}

void FarCars::Patch(const Vector3& camera, std::span<render::MeshDraw> meshes) const {
    for (const FarCar& car : cars_) {
        const Vector3 offset = car.position - camera;
        const bool isFar = Dot(offset, offset) > car.rangeSquared;
        for (std::size_t draw = 0; draw < car.draws.size() && car.firstDraw + draw < meshes.size();
             ++draw) {
            meshes[car.firstDraw + draw].indexCount = isFar ? 0 : car.draws[draw].indexCount;
        }
    }
}

void AddGridCars(const RacerGrid& racers, std::span<const sim::EntitySnapshot> poses,
                 std::size_t firstDraw, FarCars& out) {
    const std::span<const render::MeshDraw> draws = racers.Draws();
    std::size_t first = 0;
    for (std::size_t slot = 0; slot < racers.Count() && slot < poses.size(); ++slot) {
        const std::size_t count = racers.SlotDraws(slot).size();
        out.Add({.position = poses[slot].transform.position,
                 .firstDraw = firstDraw + first,
                 .draws = draws.subspan(first, count)});
        first += count;
    }
}

}
