#pragma once

#include <cstddef>
#include <cstdint>

#include "core/duration.hpp"
#include "core/vector3.hpp"
#include "sim/physics/collision_query.hpp"
#include "sim/physics/racer.hpp"
#include "sim/powerup/powerup_effect.hpp"
#include "sim/powerup/powerup_kind.hpp"

namespace rr::sim {

enum class TrapPhase : std::uint8_t { Dropping = 2, Armed = 3, Fading = 4, Done = 6 };

struct DropPoint {
    Vector3 position;
    Vector3 normal{.z = 1.0F};
};

/// 1 above the ground within 50 below `owner`, or at the car over nothing.
[[nodiscard]] DropPoint DropUnder(const Racer& owner, const CollisionQuery& world);

void KickOwnerOnDrop(Racer& owner);

struct Trap {
    Vector3 position;
    float radius = 0.0F;
    std::uint8_t owner = 0;
    std::uint8_t level = 0;
};

/// Ghosts and the owner can't touch a trap.
[[nodiscard]] bool IsTouching(const PowerupContext& context, const Trap& trap, std::size_t racer);

/// A shielded racer absorbs the touch instead of `touch`. Returns true once the trap is used up.
template <typename Touch>
[[nodiscard]] bool TouchRacers(PowerupContext& context, const Trap& trap, Touch touch) {
    for (std::size_t i = 0; i < context.racers.size(); ++i) {
        if (!IsTouching(context, trap, i)) {
            continue;
        }
        Racer& racer = context.racers[i];
        if (racer.IsShielded()) {
            racer.AbsorbShieldHit(*context.random);
            if (trap.level < kMaxPowerupLevel) {
                return true;
            }
        } else if (touch(racer, static_cast<std::uint8_t>(i))) {
            return true;
        }
    }
    return false;
}

/// Returns true, leaving `remaining` at 0, once it runs out.
[[nodiscard]] bool RunsOut(Milliseconds& remaining, Milliseconds elapsed);

}
