#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>

#include "core/bounding_sphere.hpp"
#include "core/vector3.hpp"
#include "sim/physics/racer.hpp"
#include "sim/powerup/ballistic_flight.hpp"
#include "sim/powerup/powerup_effect.hpp"

namespace rr::sim {

struct AimCone {
    float minDistanceSquared = 0.0F;
    float maxDistanceSquared = 0.0F;
    float cosine = 0.0F;
};

inline constexpr AimCone kRacerAheadCone{
    .minDistanceSquared = 100.0F, .maxDistanceSquared = 160'000.0F, .cosine = 0.9F};
inline constexpr AimCone kMissileCone{
    .minDistanceSquared = 100.0F, .maxDistanceSquared = 160'000.0F, .cosine = 0.7071F};

/// Searches in racer order from `first` like the original, so the match isn't the nearest racer.
[[nodiscard]] std::optional<std::size_t> FindRacerInCone(std::span<const Racer> racers,
                                                         const Vector3& origin,
                                                         const Vector3& direction,
                                                         const AimCone& cone,
                                                         std::size_t first = 0);

[[nodiscard]] bool Touches(const BoundingSphere& a, const BoundingSphere& b);

/// Ghosts can't be touched.
[[nodiscard]] std::optional<std::size_t> FindTouchedRacer(const PowerupContext& context,
                                                          const BoundingSphere& sphere,
                                                          std::size_t owner);

enum class HitOutcome : std::uint8_t { Absorbed, TurnedBack, Struck };

/// A shield absorbs the hit. Otherwise the victim reacts and drops a white brick.
[[nodiscard]] HitOutcome HitRacer(PowerupContext& context, const PowerupHit& hit);

enum class ShotPhase : std::uint8_t { Armed, Flying, Done };

struct ShotStep {
    PowerupKind weapon = PowerupKind::Cannonball;
    std::uint8_t owner = 0;
    FlightState state = FlightState::Flying;
    Vector3 position;
    Vector3 hitNormal;
    std::optional<std::size_t> target;
};

struct ShotOutcome {
    bool isOver = false;
    std::optional<std::uint8_t> turnedBackBy;
    std::optional<std::uint8_t> struck;
};

/// Bursts where the shot landed or on the racer it reached, unless a shield turns it back.
[[nodiscard]] ShotOutcome ResolveShot(PowerupContext& context, const ShotStep& shot);

}
