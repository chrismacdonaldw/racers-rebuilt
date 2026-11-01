#include "sim/powerup/explosions.hpp"

#include <algorithm>
#include <cmath>

#include "core/assert.hpp"
#include "sim/powerup/weapon_hits.hpp"

namespace rr::sim {
namespace {

constexpr float kWorldLimit = 2048.0F;
constexpr float kBaseRadius = 0.1F;
constexpr float kInitialGrowth = 0.05F;
constexpr float kInitialGrowthRate = 2.0F;
constexpr float kGrowthAcceleration = -2.1F;

bool IsInsideWorld(const Vector3& at) {
    return std::abs(at.x) <= kWorldLimit && std::abs(at.y) <= kWorldLimit &&
           std::abs(at.z) <= kWorldLimit;
}

void Grow(Explosion& explosion, Milliseconds elapsed) {
    const float seconds = static_cast<float>(elapsed.count()) * 0.001F;
    explosion.growthRate += kGrowthAcceleration * seconds;
    explosion.growth += explosion.growthRate * seconds;
}

Explosion* NearestEnd(std::vector<Explosion>& explosions, ExplosionPhase phase) {
    Explosion* best = nullptr;
    for (Explosion& explosion : explosions) {
        if (explosion.phase == phase &&
            (best == nullptr || explosion.remaining < best->remaining)) {
            best = &explosion;
        }
    }
    return best;
}

}

Explosions::Explosions(const ExplosionSpec& spec, std::size_t count)
    : spec_(spec), explosions_(count) {
    RR_ASSERT(count > 0);
}

void Explosions::Spawn(const Vector3& at, bool leavesScar, std::optional<std::uint8_t> owner) {
    if (!IsInsideWorld(at)) {
        return;
    }
    Claim() = {.position = at,
               .owner = owner,
               .phase = ExplosionPhase::Exploding,
               .remaining = spec_.flash,
               .growth = kInitialGrowth,
               .growthRate = kInitialGrowthRate,
               .leavesScar = leavesScar};
}

void Explosions::Tick(Milliseconds elapsed, std::span<Racer> racers,
                      std::span<const BoundingSphere> reaches) {
    for (Explosion& explosion : explosions_) {
        if (explosion.phase == ExplosionPhase::Exploding) {
            if (elapsed >= explosion.remaining) {
                explosion.phase =
                    explosion.leavesScar ? ExplosionPhase::ScarFading : ExplosionPhase::Idle;
                explosion.remaining = spec_.scar;
            } else {
                explosion.remaining -= elapsed;
                Grow(explosion, elapsed);
                Blast(explosion, racers, reaches);
            }
        }
        if (explosion.phase == ExplosionPhase::ScarFading) {
            if (elapsed >= explosion.remaining) {
                explosion.phase = ExplosionPhase::Idle;
            } else {
                explosion.remaining -= elapsed;
            }
        }
    }
}

float Explosions::BlastRadius(const Explosion& explosion) const {
    return kBaseRadius + ((spec_.blastRadius - kBaseRadius) * explosion.growth);
}

Explosion& Explosions::Claim() {
    const auto idle = std::ranges::find(explosions_, ExplosionPhase::Idle, &Explosion::phase);
    if (idle != explosions_.end()) {
        return *idle;
    }
    if (Explosion* scar = NearestEnd(explosions_, ExplosionPhase::ScarFading)) {
        return *scar;
    }
    return *NearestEnd(explosions_, ExplosionPhase::Exploding);
}

void Explosions::Blast(const Explosion& explosion, std::span<Racer> racers,
                       std::span<const BoundingSphere> reaches) const {
    const BoundingSphere blast{.centre = explosion.position, .radius = BlastRadius(explosion)};
    const float strength = std::clamp(2.0F * static_cast<float>(explosion.remaining.count()) /
                                          static_cast<float>(spec_.flash.count()),
                                      0.0F, 1.0F);
    for (std::size_t i = 0; i < racers.size(); ++i) {
        Racer& racer = racers[i];
        const bool isSkipped = explosion.owner == i || racer.IsShielded() ||
                               racer.Status().isGhost || (!racer.Route() && racer.IsSpunOut());
        if (!isSkipped && Touches(blast, reaches[i])) {
            racer.LaunchHit(strength);
        }
    }
}

}
