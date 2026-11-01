#include "app/racer/car_shadows.hpp"

#include <cstddef>
#include <format>

#include "app/track_space.hpp"
#include "core/quaternion.hpp"

namespace rr::app {
namespace {

constexpr Vector3 kGameUp{.z = 1.0F};
constexpr Vector3 kGameForward{.x = 1.0F};
constexpr float kCastAbove = 6.0F;
constexpr float kCastDepth = 15.0F;
constexpr float kSteepestGroundUp = 0.1F;

Vector3 AlongGround(const Vector3& level, const Vector3& normal) {
    return {
        .x = level.x, .y = level.y, .z = -((normal.x * level.x) + (normal.y * level.y)) / normal.z};
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
                        const sim::CollisionQuery& ground,
                        std::vector<render::QuadDraw>& out) const {
    for (std::size_t slot = 0; slot < shadows_.size(); ++slot) {
        const std::optional<Shadow>& shadow = shadows_[slot];
        if (!shadow || racers[slot].isGhost) {
            continue;
        }
        const Transform car = TransformToGame(poses[slot].transform);
        const Vector3 from = car.position + (kGameUp * kCastAbove);
        const std::optional<sim::SegmentHit> hit =
            sim::CastSolid(ground, from, from - (kGameUp * kCastDepth));
        if (!hit || hit->normal.z < kSteepestGroundUp) {
            continue;
        }
        const Vector3 heading = Rotate(car.rotation, kGameForward);
        const Vector3 forward = Normalized({.x = heading.x, .y = heading.y}, kGameForward);
        const float half = looks[slot].scale * 0.5F;
        const Vector3 top = AlongGround(forward * (shadow->length * half), hit->normal);
        const Vector3 right =
            AlongGround(Cross(forward, kGameUp) * (shadow->width * half), hit->normal);
        AppendGroundQuad(shadow->look, ToEngine(hit->point), ToEngine(hit->normal), ToEngine(top),
                         ToEngine(right), out);
    }
}

}
