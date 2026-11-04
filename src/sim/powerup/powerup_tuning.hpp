#pragma once

#include <array>
#include <cstdint>

#include "core/duration.hpp"
#include "sim/physics/turbo.hpp"
#include "sim/powerup/aim_cones.hpp"
#include "sim/powerup/ballistic_flight.hpp"
#include "sim/powerup/powerup_kind.hpp"
#include "sim/powerup/warp_rules.hpp"

namespace rr::sim {

struct CannonballTuning {
    BallisticSpec shot{.speed = 180.0F, .gravity = -32.176F, .lifetime = Milliseconds{3000}};
    BallisticSpec hazardShot{.speed = 30.0F, .gravity = -96.528F, .lifetime = Milliseconds{3000}};
    AimCone racerCone = kRacerAheadCone;
    AimCone pointCone = kTargetPointCone;
    float range = 500.0F;
};

/// `chaseSpeed` and `retractSpeed` are in units per s.
struct GrappleTuning {
    BallisticSpec hook{.speed = 320.0F, .gravity = -90.176F, .lifetime = Milliseconds{3000}};
    AimCone farCone{.minDistanceSquared = 100.0F, .maxDistanceSquared = 62'500.0F, .cosine = 0.93F};
    AimCone nearCone{
        .minDistanceSquared = 100.0F, .maxDistanceSquared = 160'000.0F, .cosine = 0.6F};
    AimCone pointCone = kTargetPointCone;
    float range = 500.0F;
    Milliseconds pullTime{4000};
    Milliseconds retractTime{500};
    float pullForce = 180.0F;
    float releaseDistanceSquared = 144.0F;
    float chaseSpeed = 300.0F;
    float retractSpeed = 250.0F;
    float tensionPerSecond = 1.0F;
};

struct LightningTuning {
    Milliseconds rampTime{500};
    Milliseconds sustainTime{7000};
    Milliseconds fadeTime{500};
    Milliseconds holdTime{1000};
    float segmentLength = 50.0F;
    AimCone victimCone{.minDistanceSquared = 9.0F, .maxDistanceSquared = 40'000.0F, .cosine = 0.5F};
    float hitStrength = 1.0F;
};

/// Indexed by shot.
struct MissileTuning {
    BallisticSpec shot{.speed = 170.0F, .gravity = -32.176F, .lifetime = Milliseconds{5500}};
    std::array<Milliseconds, kMaxLaunches> armTimes{Milliseconds{933}, Milliseconds{1200},
                                                    Milliseconds{1333}};
    std::array<float, kMaxLaunches> sideOffsets{150.0F, 0.0F, -150.0F};
    float range = 500.0F;
    float spinTurns = 2.0F;
    float spinRatePerMs = 0.007F;
};

struct OilSlickTuning {
    float radius = 3.0F;
    Milliseconds lifetime{10'000};
    float spinTurns = 1.0F;
    float spinRatePerMs = 0.007F;
    float spinAlign = 1.0F;
};

/// With no racer behind, the dynamite lands `range` behind the owner.
struct DynamiteTuning {
    BallisticSpec shot{.speed = 40.0F,
                       .gravity = -32.176F,
                       .lifetime = Milliseconds{3000},
                       .leadsOnlyBehind = true};
    AimCone targetCone = kRacerAheadCone;
    float range = 90.0F;
    Milliseconds fuse{5000};
    Milliseconds blastInterval{500};
    int scatter = 6;
};

/// `stoppedSpeed` is in units per ms.
struct MagnetTuning {
    float radius = 10.0F;
    Milliseconds armed{20'000};
    Milliseconds holding{4000};
    Milliseconds fade{1000};
    Milliseconds haltedHold{1000};
    float grabDistanceSquared = 81.0F;
    float liftStopDistanceSquared = 100.0F;
    float stoppedSpeed = 0.002F;
    float pullPerMs = 1.0F;
};

struct CurseTuning {
    float radius = 10.0F;
    Milliseconds active{15'000};
    Milliseconds fade{1000};
    Milliseconds curse{10'000};
};

/// Indexed by level.
struct ShieldTuning {
    std::array<Milliseconds, kMaxPowerupLevel + 1> durations{
        Milliseconds{4000}, Milliseconds{6000}, Milliseconds{8000}, Milliseconds{10'000}};
    Milliseconds fade{1000};
    std::uint8_t deflectLevel = 2;
};

/// Indexed by level.
struct TurboTuning {
    std::array<TurboLevel, kMaxPowerupLevel + 1> levels{TurboLevel::Weak, TurboLevel::Strong,
                                                        TurboLevel::Hover, TurboLevel::Hover};
};

struct WarpTuning {
    Milliseconds startingTime{500};
    Milliseconds activeTime{1500};
    WarpPace pace;
};

/// Defaults are the original game's values.
struct PowerupTuning {
    PowerupSlots slots = kOriginalSlots;
    CannonballTuning cannonball;
    GrappleTuning grapple;
    LightningTuning lightning;
    MissileTuning missile;
    OilSlickTuning oilSlick;
    DynamiteTuning dynamite;
    MagnetTuning magnet;
    CurseTuning curse;
    ShieldTuning shield;
    TurboTuning turbo;
    WarpTuning warp;
};

inline constexpr PowerupTuning kOriginalPowerups{};

}
