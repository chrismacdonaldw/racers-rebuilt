#include "app/powerup/cursed_cars.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

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

Vector3 StartFor(const sim::CurseSource& source, const Vector3& car,
                 std::span<const std::optional<Vector3>> dropped) {
    if (source.giver && *source.giver < dropped.size()) {
        if (const std::optional<Vector3>& given = dropped[*source.giver]) {
            return *given;
        }
    }
    return source.trap ? ToGame(*source.trap) : car + (kUp * kHoverHeight);
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
    dropped_.assign(count, std::nullopt);
    for (std::size_t i = 0; i < count; ++i) {
        std::optional<CurseOrbit>& orbit = orbits_[i];
        if (!snapshot.racers[i].isCursed && orbit) {
            dropped_[i] = orbit->Pose().position;
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
            orbit ? *orbit : orbit.emplace(StartFor(snapshot.racers[i].curseSource, car, dropped_));
        follower.Follow(car, snapshot.racers[i].forwardSpeed, elapsedMs);
    }
}

}
