#include "sim/ai/ai_aim.hpp"

#include "sim/powerup/aim_cones.hpp"

namespace rr::sim {
namespace {

constexpr AimCone kShotAimCone{
    .minDistanceSquared = 100.0F, .maxDistanceSquared = 62'500.0F, .cosine = 0.8F};
constexpr AimCone kDropAimCone{
    .minDistanceSquared = 25.0F, .maxDistanceSquared = 10'000.0F, .cosine = 0.8F};

}

std::optional<Vector3> AimFor(std::optional<assets::BrickColour> colour,
                              std::span<const Vector3> positions, std::size_t self,
                              const BodyMotion& motion) {
    const Vector3& forward = motion.basis.forward;
    std::optional<std::size_t> rival;
    if (colour == assets::BrickColour::Red) {
        rival = FindNearestPositionInCone(positions, self, motion.position, forward, kShotAimCone);
    } else if (colour == assets::BrickColour::Yellow) {
        rival = FindNearestPositionInCone(positions, self, motion.position, -forward, kDropAimCone);
    }
    return rival ? std::optional(positions[*rival]) : std::nullopt;
}

}
