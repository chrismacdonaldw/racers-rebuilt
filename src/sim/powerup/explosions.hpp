#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "core/bounding_sphere.hpp"
#include "core/duration.hpp"
#include "core/vector3.hpp"
#include "sim/physics/racer.hpp"

namespace rr::sim {

struct ExplosionSpec {
    Milliseconds flash{1000};
    Milliseconds scar{5000};
    float blastRadius = 5.0F;
};

inline constexpr std::size_t kExplosionCount = 6;
inline constexpr ExplosionSpec kSpikeExplosion{.blastRadius = 10.0F};
inline constexpr std::size_t kSpikeExplosionCount = 2;

enum class ExplosionPhase : std::uint8_t { Idle, Exploding, ScarFading };

struct Explosion {
    Vector3 position;
    std::optional<std::uint8_t> owner;
    ExplosionPhase phase = ExplosionPhase::Idle;
    Milliseconds remaining{0};
    float growth = 0.0F;
    float growthRate = 0.0F;
    bool leavesScar = false;
};

/// A fixed pool. Spawning with none idle takes back the scar, then the flash, nearest its end.
class Explosions {
public:
    explicit Explosions(const ExplosionSpec& spec = {}, std::size_t count = kExplosionCount);

    /// Ignored outside the original's ±2,048 world box.
    void Spawn(const Vector3& at, bool leavesScar, std::optional<std::uint8_t> owner);

    /// Launches every unshielded racer the blast touches, except its owner.
    void Tick(Milliseconds elapsed, std::span<Racer> racers,
              std::span<const BoundingSphere> reaches);

    [[nodiscard]] float BlastRadius(const Explosion& explosion) const;

    /// From 1 as the scar appears to 0 as it goes, and 0 for an explosion without one.
    [[nodiscard]] float ScarLeft(const Explosion& explosion) const;

    [[nodiscard]] std::span<const Explosion> All() const { return explosions_; }

private:
    [[nodiscard]] Explosion& Claim();
    void Blast(const Explosion& explosion, std::span<Racer> racers,
               std::span<const BoundingSphere> reaches) const;

    ExplosionSpec spec_;
    std::vector<Explosion> explosions_;
};

}
