#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

#include "assets/event_id.hpp"
#include "core/duration.hpp"
#include "core/result.hpp"
#include "core/vector3.hpp"

namespace rr::assets {

enum class HazardKind : std::uint8_t {
    FallingPillar = 0x28,
    Sphinx = 0x29,
    HammerSwish = 0x2A,
    Ghost = 0x2B,
    LavaBlob = 0x2C,
    CodePuzzle = 0x2D,
    RocketPad = 0x2E,
    Snowfall = 0x2F,
    SmokeVent = 0x30,
    Crane = 0x32,
    Cannon = 0x33,
    BreakableProp = 0x34,
    WaterScroll = 0x36,
    ShipCannons = 0x3D,
    SwingingBox = 0x3E,
    CurseDrop = 0x3F,
    LightningCannon = 0x40,
    DragonCannonballs = 0x43,
    UfoGrabber = 0x48,
    WarpPad = 0x49,
};

inline constexpr std::size_t kMaxPropEntities = 4;

struct CannonShot {
    Vector3 launch;
    std::optional<Vector3> target;
    std::optional<Vector3> triggerCentre;
    float triggerRadius = 0.0F;
    EventId event = kNoEvent;

    friend bool operator==(const CannonShot&, const CannonShot&) = default;
};

struct BreakableProp {
    std::string surface;
    std::vector<std::string> entities;
    EventId trigger = kNoEvent;

    friend bool operator==(const BreakableProp&, const BreakableProp&) = default;
};

struct WaterScroll {
    std::string entity;
    float uAmplitude = 0.0F;
    float vAmplitude = 0.0F;

    friend bool operator==(const WaterScroll&, const WaterScroll&) = default;
};

struct AimPoint {
    Vector3 position;
    EventId event = kNoEvent;

    friend bool operator==(const AimPoint&, const AimPoint&) = default;
};

struct ShipCannons {
    std::vector<AimPoint> launches;
    std::vector<AimPoint> targets;
    CannonShot shot;

    friend bool operator==(const ShipCannons&, const ShipCannons&) = default;
};

struct SwingingBox {
    std::string entity;
    EventId trigger = kNoEvent;
    float startFrame = 0.0F;
    Vector3 size;

    friend bool operator==(const SwingingBox&, const SwingingBox&) = default;
};

/// Aims from `entity` when it is set, else from `source`.
struct LightningCannon {
    EventId trigger = kNoEvent;
    std::optional<std::string> entity;
    std::optional<Vector3> source;
    Milliseconds period{0};
    float sweepRadians = 0.0F;
    float verticalRange = 0.0F;
    float verticalBase = 0.0F;

    friend bool operator==(const LightningCannon&, const LightningCannon&) = default;
};

struct DragonCannonballs {
    std::optional<std::string> material;
    std::string entity;
    EventId trigger = kNoEvent;
    Milliseconds interval{0};

    friend bool operator==(const DragonCannonballs&, const DragonCannonballs&) = default;
};

struct UfoGrabber {
    EventId trigger = kNoEvent;
    std::string entity;
    float pullStrength = 0.0F;
    float grabFrameLow = 0.0F;
    float grabFrameHigh = 0.0F;

    friend bool operator==(const UfoGrabber&, const UfoGrabber&) = default;
};

using HazardPayload =
    std::variant<std::monostate, CannonShot, BreakableProp, WaterScroll, ShipCannons, SwingingBox,
                 LightningCannon, DragonCannonballs, UfoGrabber>;

/// Kinds whose data lives in the exe carry `std::monostate`.
struct Hazard {
    HazardKind kind = HazardKind::FallingPillar;
    HazardPayload payload;
};

/// Reads `HAZARDS.HZB`. An unknown hazard class fails.
[[nodiscard]] Result<std::vector<Hazard>> ParseHazards(std::span<const std::byte> bytes);

}
