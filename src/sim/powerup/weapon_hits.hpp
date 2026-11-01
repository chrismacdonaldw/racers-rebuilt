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

inline constexpr float kShotTouchRadius = 2.5F;

[[nodiscard]] bool Touches(const BoundingSphere& a, const BoundingSphere& b);
[[nodiscard]] bool Touches(const Vector3& from, const Vector3& to, const BoundingSphere& sphere);

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
