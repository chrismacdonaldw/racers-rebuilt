#include "sim/physics/contact_effects.hpp"

#include <cmath>
#include <cstdint>
#include <optional>

namespace rr::sim {
namespace {

constexpr float kShoveAlignment = 0.7F;
constexpr float kShoveForce = 200.0F;
constexpr float kShieldSpinRate = 0.009F;

void ShieldActsOn(std::uint8_t level, Racer& other, const Vector3& point) {
    switch (level) {
        case 1: {
            const Vector3 away = Normalized(other.CentreOfMass() - point, {});
            if (std::abs(Dot(other.Body().motion.basis.forward, away)) < kShoveAlignment) {
                other.Shove(away * kShoveForce);
            }
            break;
        }
        case 2:
            other.StartSpin(1.0F, kShieldSpinRate);
            break;
        case 3:
            other.StartSpin(2.0F, kShieldSpinRate);
            break;
        default:
            break;
    }
}

bool CanPassCurse(const Racer& from, const Racer& to) {
    return from.Status().isCursed && from.Status().curseMs > 0 && !to.Status().isCursed &&
           !to.IsShielded();
}

void PassCurse(Racer& from, Racer& to) {
    to.AttachCurse(Milliseconds{from.Status().curseMs});
    from.RemoveCurse();
}

}

void ApplyContactEffects(Racer& first, Racer& second, const Vector3& point) {
    if (const std::optional<std::uint8_t> level = first.Status().shield.level;
        level && !second.IsShielded()) {
        ShieldActsOn(*level, second, point);
    } else if (const std::optional<std::uint8_t> other = second.Status().shield.level;
               other && !first.IsShielded()) {
        ShieldActsOn(*other, first, point);
    }
    if (CanPassCurse(first, second)) {
        PassCurse(first, second);
    } else if (CanPassCurse(second, first)) {
        PassCurse(second, first);
    }
}

}
