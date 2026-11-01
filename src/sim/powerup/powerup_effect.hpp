#pragma once

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <span>

#include "assets/target_points.hpp"
#include "core/bounding_sphere.hpp"
#include "core/duration.hpp"
#include "core/transform.hpp"
#include "sim/physics/collision_query.hpp"
#include "sim/physics/racer.hpp"
#include "sim/powerup/powerup_inventory.hpp"
#include "sim/powerup/powerup_kind.hpp"
#include "sim/race/checkpoint_graph.hpp"
#include "sim/race/race_progress.hpp"
#include "sim/race/random.hpp"

namespace rr::sim {

class BrickField;
class Explosions;
class PowerupSystem;
class TrackEvents;
class TrackSurfaces;
class WeaponEvents;

/// Everything it points to outlives the call. `reaches` are the racers' bounding spheres.
struct PowerupContext {
    std::span<Racer> racers;
    const CollisionQuery* world = nullptr;
    Random* random = nullptr;
    const CheckpointGraph* checkpoints = nullptr;
    const RaceProgress* progress = nullptr;
    std::span<const BoundingSphere> reaches;
    PowerupSystem* powerups = nullptr;
    BrickField* bricks = nullptr;
    std::span<PowerupInventory> inventories;
    Explosions* explosions = nullptr;
    Explosions* spikes = nullptr;
    WeaponEvents* weapons = nullptr;
    const TrackSurfaces* surfaces = nullptr;
    TrackEvents* trackEvents = nullptr;
    std::span<const assets::TargetPoint> targets;
};

struct PowerupHit {
    std::uint8_t racer = 0;
    std::uint8_t from = 0;
    PowerupKind by = PowerupKind::Cannonball;
};

/// When a pool runs dry, the manager takes back the highest phase, then the lowest phase time.
struct EffectProgress {
    std::uint8_t phase = 0;
    Milliseconds phaseTime{0};
    bool isHoldingRacer = false;

    friend bool operator==(const EffectProgress&, const EffectProgress&) = default;
};

class PowerupEffect {
public:
    virtual ~PowerupEffect() = default;

    virtual void Start(const PowerupUse& use, PowerupContext& context) = 0;

    /// Returns false once the effect is over. It ends after every effect has ticked.
    [[nodiscard]] virtual bool Tick(Milliseconds elapsed, PowerupContext& context) = 0;

    /// Asked when the racer that used this effect is hit. Returns true to stop the hit.
    [[nodiscard]] virtual bool Hit(const PowerupHit& hit, PowerupContext& context) = 0;

    /// Runs once as the effect leaves its pool, whether it finished or was taken back.
    virtual void End(PowerupContext& context) = 0;

    [[nodiscard]] virtual EffectProgress Progress() const = 0;

    /// In game space.
    [[nodiscard]] virtual Transform Pose() const = 0;
};

using EffectFactory = std::function<std::unique_ptr<PowerupEffect>()>;

/// Kinds without a factory do nothing when used.
using EffectFactories = std::array<EffectFactory, kPowerupKindCount>;

}
