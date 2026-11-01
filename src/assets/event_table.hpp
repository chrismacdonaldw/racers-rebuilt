#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <vector>

#include "assets/event_id.hpp"
#include "core/duration.hpp"
#include "core/result.hpp"
#include "core/vector3.hpp"

namespace rr::assets {

/// Events a resource starts on entering a phase and ends on leaving it.
struct SubEvents {
    EventId start = kNoEvent;
    EventId active = kNoEvent;
    EventId end = kNoEvent;

    friend bool operator==(const SubEvents&, const SubEvents&) = default;
};

struct EventTrigger {
    EventId event = 0;
    bool startsOnEnd = false;
    bool ignoresEnd = false;
    bool usesEventPosition = false;
    SubEvents subEvents;
};

/// `isShared` looks the name up in the shared database instead of the track's.
struct EntityRef {
    std::string name;
    bool isShared = false;

    friend bool operator==(const EntityRef&, const EntityRef&) = default;
};

/// `probability` below 1 plays now and then while the event runs, instead of on its start.
struct SoundResource {
    EventTrigger trigger;
    std::optional<Vector3> position;
    std::int32_t soundId = 0;
    float volume = 1.0F;
    float frequencyScale = 1.0F;
    float minDistance = 30.0F;
    float maxDistance = 300.0F;
    bool isLooping = false;
    float probability = 1.0F;
    std::optional<std::string> entity;
    std::int32_t node = 0;
};

struct PartAnimationResource {
    EventTrigger trigger;
    std::optional<EntityRef> entity;
    std::int32_t activePart = 0;
    std::int32_t idlePart = 0;
    std::int32_t startPart = -1;
    std::int32_t endPart = -1;
    bool isLooping = false;
};

struct EntityMaterial {
    std::string entity;
    std::int32_t material = 0;
};

struct MaterialAnimationResource {
    EventTrigger trigger;
    std::optional<std::int32_t> animation;
    std::int32_t activeTrack = 0;
    std::int32_t idleTrack = 0;
    bool isLooping = false;
    std::optional<EntityMaterial> material;
};

struct ParticleResource {
    EventTrigger trigger;
    std::string emitter;
    Vector3 position;
    Vector3 direction{.x = 1.0F};
    Vector3 up{.z = 1.0F};
    std::optional<EntityRef> entity;
    std::int32_t node = 0;
};

/// Starting `event` forces every resource on `forced` to end.
struct EventLink {
    EventId event = 0;
    EventId forced = 0;
};

struct SkyStateResource {
    EventTrigger trigger;
    std::string sky;
    Milliseconds fade{0};
    bool hidesDome = false;
    bool showsDome = false;
    bool hidesSkyWorld = false;
    bool showsSkyWorld = false;
};

/// Doesn't run out while `holdEvent` runs.
struct TimerResource {
    EventTrigger trigger;
    Milliseconds duration{0};
    EventId holdEvent = kNoEvent;
};

/// Moves the bounded `triggerEntity` onto node `node` of the animated `entity`.
struct NodeTransformResource {
    EventTrigger trigger;
    std::optional<std::string> triggerEntity;
    std::optional<std::string> entity;
    std::int32_t node = 0;
};

/// Each colour channel becomes `min(255, (c >> shift) + offset)`. No entity tints the racer.
struct ColorTransformResource {
    EventTrigger trigger;
    std::optional<std::string> entity;
    std::array<std::int32_t, 4> shifts{};
    std::array<std::int32_t, 4> offsets{};
    bool clears = false;
};

enum class LapZone : std::uint8_t { OpenTrack, FinishLine, PostLine };

struct LapZoneResource {
    EventTrigger trigger;
    LapZone zone = LapZone::FinishLine;
};

struct ModelDistanceResource {
    EventTrigger trigger;
    std::optional<EntityRef> entity;
    bool hidesWhileActive = false;
};

struct LookTargetResource {
    EventTrigger trigger;
    Vector3 position;
};

/// Does nothing until `armEvent` starts.
struct ExternalForceResource {
    EventTrigger trigger;
    Vector3 force;
    std::int32_t channel = 0;
    EventId armEvent = kNoEvent;
};

struct EventTableAsset {
    std::vector<SoundResource> sounds;
    std::vector<PartAnimationResource> partAnimations;
    std::vector<MaterialAnimationResource> materialAnimations;
    std::vector<ParticleResource> particles;
    std::vector<EventLink> links;
    std::vector<SkyStateResource> skyStates;
    std::vector<TimerResource> timers;
    std::vector<NodeTransformResource> nodeTransforms;
    std::vector<ColorTransformResource> colorTransforms;
    std::vector<LapZoneResource> lapZones;
    std::vector<ModelDistanceResource> modelDistances;
    std::vector<LookTargetResource> lookTargets;
    std::vector<ExternalForceResource> externalForces;
};

/// Positions stay as the file has them, unmirrored.
[[nodiscard]] Result<EventTableAsset> ParseEventTable(std::span<const std::byte> bytes);

}
