#pragma once

#include <array>
#include <cstdint>
#include <optional>
#include <vector>

#include "assets/brick_layout.hpp"
#include "core/duration.hpp"
#include "core/transform.hpp"
#include "sim/animation/part_clock.hpp"
#include "sim/hazard/code_gates.hpp"
#include "sim/physics/curse_source.hpp"
#include "sim/physics/turbo.hpp"
#include "sim/powerup/powerup_kind.hpp"
#include "sim/time_trial/ghost_playback.hpp"

namespace rr::sim {

enum class EntityId : std::uint32_t {};

enum class SnapshotFlags : std::uint8_t {
    None = 0,
    Teleported = 1U << 0U,
};

[[nodiscard]] constexpr SnapshotFlags operator|(SnapshotFlags a, SnapshotFlags b) {
    return static_cast<SnapshotFlags>(static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b));
}

[[nodiscard]] constexpr bool HasFlag(SnapshotFlags flags, SnapshotFlags flag) {
    return (static_cast<std::uint8_t>(flags) & static_cast<std::uint8_t>(flag)) != 0;
}

struct EntitySnapshot {
    EntityId id{};
    Transform transform;
    SnapshotFlags flags = SnapshotFlags::None;

    friend bool operator==(const EntitySnapshot&, const EntitySnapshot&) = default;
};

struct CameraSnapshot {
    Transform transform;
    SnapshotFlags flags = SnapshotFlags::None;

    friend bool operator==(const CameraSnapshot&, const CameraSnapshot&) = default;
};

/// Each is nothing while that ghost isn't drawn.
struct GhostSnapshot {
    std::optional<GhostFrame> best;
    std::optional<GhostFrame> record;

    friend bool operator==(const GhostSnapshot&, const GhostSnapshot&) = default;
};

struct SkidSnapshot {
    std::optional<TurboLevel> turbo;
    std::uint32_t turboCount = 0;
    float leftwardSpeed = 0.0F;
    bool isDrifting = false;
    bool isSpinning = false;
    bool isMagnetHeld = false;
    bool isHalted = false;

    friend bool operator==(const SkidSnapshot&, const SkidSnapshot&) = default;
};

/// `turnRadius` is in world units, positive turning left and 0 when driving straight.
/// `hitReactions` and `suddenStops` only count up. `finishPlace` waits for the last lap.
/// `curseSource.trap` is in engine space.
struct RacerSnapshot {
    EntityId id{};
    float forwardSpeed = 0.0F;
    float turnRadius = 0.0F;
    std::uint32_t hitReactions = 0;
    std::uint32_t suddenStops = 0;
    bool isReversing = false;
    std::optional<int> finishPlace;
    std::optional<assets::BrickColour> heldColour;
    std::uint8_t whiteCount = 0;
    bool isGhost = false;
    bool isCursed = false;
    CurseSource curseSource;
    std::optional<std::uint8_t> tint;
    SkidSnapshot skid;

    friend bool operator==(const RacerSnapshot&, const RacerSnapshot&) = default;
};

struct BrickSnapshot {
    Transform transform;
    float scale = 0.0F;
    std::optional<assets::BrickColour> colour;

    friend bool operator==(const BrickSnapshot&, const BrickSnapshot&) = default;
};

struct EffectSnapshot {
    std::uint16_t slot = 0;
    PowerupKind kind = PowerupKind::Cannonball;
    PowerupUse use;
    std::uint8_t phase = 0;
    float phaseLeftMs = 0.0F;
    bool isHoldingRacer = false;
    Transform transform;
    std::optional<Vector3> hazardSource;
    SnapshotFlags flags = SnapshotFlags::None;

    friend bool operator==(const EffectSnapshot&, const EffectSnapshot&) = default;
};

enum class ExplosionKind : std::uint8_t { Blast, Spike };

/// `growth` is 0 once the flash ends, and `scarLeft` falls from 1 to 0 as the scar fades.
struct ExplosionSnapshot {
    ExplosionKind kind = ExplosionKind::Blast;
    Transform transform;
    float growth = 0.0F;
    float scarLeft = 0.0F;

    friend bool operator==(const ExplosionSnapshot&, const ExplosionSnapshot&) = default;
};

/// `areLampsRed` is nothing without a code puzzle. The times count up from the solve and the hit.
struct HazardMaterialsSnapshot {
    std::optional<std::array<bool, kCodeGates>> areLampsRed;
    std::optional<Milliseconds> lampsFlashing;
    std::optional<Milliseconds> sphinxBlowingUp;

    friend bool operator==(const HazardMaterialsSnapshot&,
                           const HazardMaterialsSnapshot&) = default;
};

/// `entities` stays sorted by id. Reuse a snapshot between ticks so its storage isn't reallocated.
struct RenderSnapshot {
    std::uint64_t tick = 0;
    std::vector<EntitySnapshot> entities;
    std::vector<RacerSnapshot> racers;
    std::vector<BrickSnapshot> bricks;
    std::vector<EffectSnapshot> effects;
    std::vector<ExplosionSnapshot> explosions;
    std::vector<std::optional<PartClock>> entityClocks;
    std::vector<bool> hiddenModels;
    HazardMaterialsSnapshot hazardMaterials;
    CameraSnapshot camera;
    std::vector<CameraSnapshot> otherPlayerCameras;
    GhostSnapshot ghosts;
    std::uint64_t simStateHash = 0;

    friend bool operator==(const RenderSnapshot&, const RenderSnapshot&) = default;
};

}
