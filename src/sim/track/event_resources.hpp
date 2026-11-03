#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <vector>

#include "assets/event_table.hpp"
#include "core/duration.hpp"
#include "sim/collision/collision_world.hpp"
#include "sim/collision/track_collision.hpp"
#include "sim/track/race_sky.hpp"
#include "sim/track/track_entities.hpp"
#include "sim/track/track_events.hpp"

namespace rr::sim {

enum class ResourcePhase : std::uint8_t { Idle, Starting, Active, Ending, EndPending };

struct TimerState {
    ResourcePhase phase = ResourcePhase::Idle;
    Milliseconds left{0};
    bool isHeld = false;

    friend bool operator==(const TimerState&, const TimerState&) = default;
};

/// `table`, `entities`, `collision` and `sky` must outlive it.
class EventResources {
public:
    EventResources(const assets::EventTableAsset& table, const TrackEntitySources& entities,
                   const TrackCollision* collision, const assets::SkyFile& sky);

    [[nodiscard]] static std::size_t MostFiredPerTick(const assets::EventTableAsset& table);

    void BeginTick() {
        dispatched_ = 0;
        subEventsFired_ = 0;
    }

    /// Applies new position-path events and their sub-events, dropping any past `MostFiredPerTick`.
    void Dispatch(TrackEvents& events, TrackEntities& entities);

    void Update(Milliseconds elapsed, TrackEvents& events, TrackEntities& entities);

    [[nodiscard]] std::span<const ResourcePhase> PartAnimations() const { return parts_; }
    [[nodiscard]] std::span<const TimerState> Timers() const { return timers_; }
    [[nodiscard]] std::span<const ResourcePhase> NodeTransforms() const { return nodes_; }
    [[nodiscard]] std::span<const ResourcePhase> ModelDistances() const { return distances_; }
    [[nodiscard]] std::span<const ResourcePhase> SkyStates() const { return skyStates_; }
    [[nodiscard]] const RaceSky& Sky() const { return sky_; }

    /// Indexed by collision world. A world moved by a node transform stays where it was left.
    [[nodiscard]] std::span<const std::optional<Placement>> MovedWorlds() const { return moved_; }

    [[nodiscard]] bool IsModelHidden(std::size_t distance) const;

private:
    struct NodeTarget {
        std::optional<std::size_t> entity;
        std::optional<std::size_t> world;
    };

    void Apply(const TrackEvent& event, TrackEvents& events, TrackEntities& entities);
    void Rephase(const assets::EventTrigger& trigger, ResourcePhase from, ResourcePhase to,
                 TrackEvents& events);
    void FireSubEvent(const TrackEvent& event, TrackEvents& events);
    void Force(assets::EventId id);

    void StartPart(std::size_t i, TrackEntities& entities);
    void EndPart(std::size_t i, TrackEntities& entities);
    void UpdatePart(std::size_t i, TrackEvents& events, TrackEntities& entities);

    void StartTimer(std::size_t i, TrackEvents& events);
    void EndTimer(std::size_t i, TrackEvents& events);
    void UpdateTimer(std::size_t i, Milliseconds elapsed, TrackEvents& events);

    void StartSky(std::size_t i);
    void EndSky(std::size_t i, TrackEvents& events);

    void StartNode(std::size_t i, TrackEvents& events);
    void EndNode(std::size_t i, TrackEvents& events);
    void UpdateNode(std::size_t i, TrackEvents& events, const TrackEntities& entities);

    const assets::EventTableAsset* table_;
    std::vector<std::optional<std::size_t>> partEntities_;
    std::vector<NodeTarget> nodeTargets_;
    std::vector<ResourcePhase> parts_;
    std::vector<TimerState> timers_;
    std::vector<ResourcePhase> nodes_;
    std::vector<ResourcePhase> distances_;
    std::vector<ResourcePhase> skyStates_;
    RaceSky sky_;
    std::vector<std::optional<Placement>> moved_;
    std::size_t dispatched_ = 0;
    std::size_t subEventBudget_;
    std::size_t subEventsFired_ = 0;
    bool hasWarnedOfCascade_ = false;
};

}
