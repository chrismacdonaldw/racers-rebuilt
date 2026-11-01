#include "app/powerup/cursed_cars.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>

#include "app/track_space.hpp"
#include "sim/physics/rigid_body.hpp"

namespace rr::app {
namespace {

constexpr float kOrbitMs = 1000.0F;
constexpr float kOrbitRadius = 6.0F;
constexpr float kHoverHeight = 9.0F;
constexpr float kSlowestChase = 0.1F;
constexpr float kShrinkPerMs = 0.01F * 0.066F;
constexpr float kSmallestScale = 0.666F;
constexpr Vector3 kUp{.z = 1.0F};

Vector3 CarAt(const sim::RenderSnapshot& snapshot, std::size_t racer) {
    return TransformToGame(snapshot.entities.at(racer).transform).position;
}

Vector3 StartFor(const sim::RenderSnapshot& snapshot, const Vector3& car,
                 const std::optional<Vector3>& dropped) {
    if (dropped) {
        return *dropped;
    }
    std::optional<Vector3> nearest;
    for (const sim::EffectSnapshot& effect : snapshot.effects) {
        if (effect.kind != sim::PowerupKind::Curse) {
            continue;
        }
        const Vector3 curse = TransformToGame(effect.transform).position;
        if (!nearest || Length(curse - car) < Length(*nearest - car)) {
            nearest = curse;
        }
    }
    return nearest.value_or(car + (kUp * kHoverHeight));
}

}

void CurseOrbit::Follow(const Vector3& car, float speed, float elapsedMs) {
    phaseMs_ = elapsedMs > phaseMs_ ? kOrbitMs : phaseMs_ - elapsedMs;
    const float phase = 2.0F * std::numbers::pi_v<float> * phaseMs_ / kOrbitMs;
    const Vector3 target = car + Vector3{.x = kOrbitRadius * std::sin(phase),
                                         .y = kOrbitRadius * std::cos(phase),
                                         .z = kHoverHeight};
    const float reach = elapsedMs * std::max(std::abs(speed), kSlowestChase);
    const Vector3 toTarget = target - position_;
    position_ = Length(toTarget) > reach ? position_ + (Normalized(toTarget, {}) * reach) : target;
    const Vector3 side = Normalized(Cross(kUp, car - position_), {});
    if (Dot(side, side) > 0.0F) {
        facing_ = sim::ToQuaternion(sim::OrthonormalBasis(side, kUp));
    }
    scale_ = std::max(kSmallestScale, scale_ - (elapsedMs * kShrinkPerMs));
}

void CursedCars::Update(const sim::RenderSnapshot& snapshot, float elapsedMs) {
    const std::size_t count = std::min(snapshot.racers.size(), snapshot.entities.size());
    orbits_.resize(count);
    std::optional<Vector3> dropped;
    for (std::size_t i = 0; i < count; ++i) {
        std::optional<CurseOrbit>& orbit = orbits_[i];
        if (!snapshot.racers[i].isCursed && orbit) {
            dropped = orbit->Pose().position;
            orbit.reset();
        }
    }
    for (std::size_t i = 0; i < count; ++i) {
        if (!snapshot.racers[i].isCursed) {
            continue;
        }
        const Vector3 car = CarAt(snapshot, i);
        std::optional<CurseOrbit>& orbit = orbits_[i];
        CurseOrbit& follower =
            orbit ? *orbit
                  : orbit.emplace(StartFor(snapshot, car, std::exchange(dropped, std::nullopt)));
        follower.Follow(car, snapshot.racers[i].forwardSpeed, elapsedMs);
    }
}

}
