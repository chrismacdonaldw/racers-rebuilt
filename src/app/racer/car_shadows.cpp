#include "app/racer/car_shadows.hpp"

#include <cstddef>
#include <format>

#include "app/track_space.hpp"
#include "core/quaternion.hpp"

namespace rr::app {
namespace {

constexpr Vector3 kGameForward{.x = 1.0F};
constexpr Vector3 kProbeAbove{.z = 6.0F};
constexpr float kFadeNearSquared = 2500.0F;
constexpr float kFadeFarSquared = 10000.0F;
constexpr float kFullAlpha = 191.0F;
constexpr float kChannelMax = 255.0F;

const Vector3* CameraFor(std::span<const Vector3> cameras, std::size_t slot) {
    if (cameras.empty()) {
        return nullptr;
    }
    return slot < cameras.size() ? &cameras[slot] : &cameras.front();
}

}

std::uint8_t ShadowAlpha(float distanceSquared) {
    if (distanceSquared <= kFadeNearSquared) {
        return static_cast<std::uint8_t>(kFullAlpha);
    }
    if (distanceSquared >= kFadeFarSquared) {
        return 0;
    }
    const float faded =
        (distanceSquared - kFadeNearSquared) / (kFadeFarSquared - kFadeNearSquared) * kFullAlpha;
    return static_cast<std::uint8_t>(static_cast<int>(kFullAlpha) - static_cast<int>(faded));
}

CarShadows::CarShadows(const assets::NameMap<render::Material>& materials,
                       std::span<const ShadowCaster> casters) {
    shadows_.reserve(casters.size());
    for (std::size_t slot = 0; slot < casters.size(); ++slot) {
        const auto found = materials.find(std::format("CARSHAD{}", slot));
        if (found == materials.end()) {
            shadows_.emplace_back();
            continue;
        }
        const ShadowCaster& caster = casters[slot];
        QuadLook look = LookOf(found->second);
        look.texture = caster.silhouette.value_or(look.texture);
        shadows_.emplace_back(Shadow{.look = look,
                                     .along = caster.chassis.shadowWidth,
                                     .across = caster.chassis.shadowLength,
                                     .centre = caster.centre});
    }
}

void CarShadows::Append(std::span<const sim::EntitySnapshot> poses,
                        std::span<const sim::RacerSnapshot> racers, std::span<const CarLook> looks,
                        std::span<const Vector3> cameras, GroundDecals& ground,
                        std::vector<render::QuadDraw>& out) const {
    for (std::size_t slot = 0; slot < shadows_.size(); ++slot) {
        const std::optional<Shadow>& shadow = shadows_[slot];
        if (!shadow || racers[slot].isGhost) {
            continue;
        }
        const Transform car = TransformToGame(poses[slot].transform);
        const float scale = looks[slot].scale;
        const Vector3 centre = car.position + Rotate(car.rotation, shadow->centre * scale);
        QuadLook look = shadow->look;
        if (const Vector3* camera = CameraFor(cameras, slot)) {
            const Vector3 offset = centre + kProbeAbove - ToGame(*camera);
            const std::uint8_t alpha = ShadowAlpha(Dot(offset, offset));
            if (alpha == 0) {
                continue;
            }
            look.tint.a = static_cast<float>(alpha) / kChannelMax;
        }
        ground.Append(look,
                      {.centre = centre,
                       .forward = Rotate(car.rotation, kGameForward),
                       .length = shadow->along * scale,
                       .width = shadow->across * scale},
                      out);
    }
}

}
