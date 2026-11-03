#include "app/racer/car_shadows.hpp"

#include <cstddef>
#include <format>

#include "app/racer/far_cars.hpp"
#include "app/track_space.hpp"
#include "core/quaternion.hpp"

namespace rr::app {
namespace {

constexpr Vector3 kGameForward{.x = 1.0F};

bool IsInCastRange(const Vector3& car, std::span<const Vector3> cameras, std::size_t slot) {
    if (cameras.empty()) {
        return true;
    }
    const Vector3& camera = slot < cameras.size() ? cameras[slot] : cameras.front();
    const Vector3 offset = car - camera;
    return Dot(offset, offset) < kCarRangeSquared;
}

}

CarShadows::CarShadows(const assets::NameMap<render::Material>& materials,
                       std::span<const sim::ChassisSpec> chassis) {
    shadows_.reserve(chassis.size());
    for (std::size_t slot = 0; slot < chassis.size(); ++slot) {
        const auto found = materials.find(std::format("CARSHAD{}", slot));
        if (found == materials.end()) {
            shadows_.emplace_back();
            continue;
        }
        shadows_.emplace_back(Shadow{.look = LookOf(found->second),
                                     .width = chassis[slot].shadowWidth,
                                     .length = chassis[slot].shadowLength});
    }
}

void CarShadows::Append(std::span<const sim::EntitySnapshot> poses,
                        std::span<const sim::RacerSnapshot> racers, std::span<const CarLook> looks,
                        std::span<const Vector3> cameras, GroundDecals& ground,
                        std::vector<render::QuadDraw>& out) const {
    for (std::size_t slot = 0; slot < shadows_.size(); ++slot) {
        const std::optional<Shadow>& shadow = shadows_[slot];
        if (!shadow || racers[slot].isGhost ||
            !IsInCastRange(poses[slot].transform.position, cameras, slot)) {
            continue;
        }
        const Transform car = TransformToGame(poses[slot].transform);
        const float scale = looks[slot].scale;
        ground.Append(shadow->look,
                      {.centre = car.position,
                       .forward = Rotate(car.rotation, kGameForward),
                       .length = shadow->length * scale,
                       .width = shadow->width * scale},
                      out);
    }
}

}
