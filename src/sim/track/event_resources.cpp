#include "sim/track/event_resources.hpp"

#include <algorithm>
#include <functional>
#include <string>
#include <string_view>

#include "assets/game_path.hpp"
#include "core/log.hpp"
#include "sim/physics/rigid_body.hpp"

namespace rr::sim {
namespace {

constexpr std::size_t kMostSubEventsPerResource = 8;

bool CanStart(ResourcePhase phase) {
    return phase == ResourcePhase::Idle || phase == ResourcePhase::Ending;
}

bool HasSubEvents(const assets::EventTrigger& trigger) {
    const assets::SubEvents& subs = trigger.subEvents;
    return subs.start != assets::kNoEvent || subs.active != assets::kNoEvent ||
           subs.end != assets::kNoEvent;
}

template <typename Start>
void OnStart(const assets::EventTrigger& trigger, ResourcePhase phase, Start start) {
    if (!trigger.startsOnEnd && CanStart(phase)) {
        start();
    }
}

template <typename Start, typename End>
void OnEnd(const assets::EventTrigger& trigger, ResourcePhase phase, Start start, End end) {
    if (trigger.startsOnEnd && CanStart(phase)) {
        start();
        if (!trigger.ignoresEnd) {
            end();
        }
    } else if (phase != ResourcePhase::Idle && !trigger.ignoresEnd) {
        end();
    }
}

assets::EventId SubEventOf(const assets::SubEvents& subs, ResourcePhase phase) {
    switch (phase) {
        case ResourcePhase::Starting:
            return subs.start;
        case ResourcePhase::Active:
            return subs.active;
        case ResourcePhase::Ending:
            return subs.end;
        default:
            return assets::kNoEvent;
    }
}

template <typename Range, typename Projection>
std::optional<std::size_t> FindNamed(const Range& range, std::string_view name,
                                     Projection projection) {
    const std::string wanted = assets::ToUpperAscii(name);
    const auto found = std::ranges::find_if(range, [&](const auto& item) {
        return assets::ToUpperAscii(std::invoke(projection, item)) == wanted;
    });
    if (found == std::ranges::end(range)) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(found - std::ranges::begin(range));
}

bool IsPart(const assets::AnimationAsset& animation, std::int32_t part, bool isOptional) {
    return (isOptional && part == -1) ||
           (part >= 0 && static_cast<std::size_t>(part) < animation.parts.size());
}

std::optional<std::size_t> PartEntity(const assets::PartAnimationResource& resource,
                                      const TrackEntitySources& sources) {
    if (!resource.entity) {
        return std::nullopt;
    }
    const std::optional<std::size_t> entity =
        FindNamed(sources.entities, resource.entity->name, &TrackEntity::name);
    if (!entity) {
        return std::nullopt;
    }
    const assets::AnimationAsset& animation =
        sources.animations[sources.entities[*entity].animation];
    const bool hasParts = IsPart(animation, resource.activePart, false) &&
                          IsPart(animation, resource.idlePart, false) &&
                          IsPart(animation, resource.startPart, true) &&
                          IsPart(animation, resource.endPart, true);
    return hasParts ? entity : std::nullopt;
}

std::optional<std::size_t> NodeEntity(const assets::NodeTransformResource& resource,
                                      const TrackEntitySources& sources) {
    if (!resource.entity) {
        return std::nullopt;
    }
    const std::optional<std::size_t> entity =
        FindNamed(sources.entities, *resource.entity, &TrackEntity::name);
    if (!entity || resource.node < 0 ||
        static_cast<std::size_t>(resource.node) >=
            sources.skeletons[sources.entities[*entity].skeleton].nodes.size()) {
        return std::nullopt;
    }
    return entity;
}

}

EventResources::EventResources(const assets::EventTableAsset& table,
                               const TrackEntitySources& entities, const TrackCollision* collision,
                               const assets::SkyFile& sky)
    : table_(&table),
      parts_(table.partAnimations.size()),
      timers_(table.timers.size()),
      nodes_(table.nodeTransforms.size()),
      distances_(table.modelDistances.size()),
      skyStates_(table.skyStates.size()),
      sky_(sky),
      subEventBudget_(MostFiredPerTick(table)) {
    for (const assets::PartAnimationResource& resource : table.partAnimations) {
        partEntities_.push_back(PartEntity(resource, entities));
    }
    for (const assets::NodeTransformResource& resource : table.nodeTransforms) {
        NodeTarget& target = nodeTargets_.emplace_back();
        target.entity = NodeEntity(resource, entities);
        if (collision != nullptr && resource.triggerEntity) {
            target.world = FindNamed(collision->worlds, *resource.triggerEntity, &NamedWorld::name);
        }
    }
    if (collision != nullptr) {
        moved_.resize(collision->worlds.size());
    }
}

std::size_t EventResources::MostFiredPerTick(const assets::EventTableAsset& table) {
    const auto count = [](const auto& resources) {
        return static_cast<std::size_t>(std::ranges::count_if(
            resources, [](const auto& resource) { return HasSubEvents(resource.trigger); }));
    };
    return kMostSubEventsPerResource * (count(table.partAnimations) + count(table.timers) +
                                        count(table.nodeTransforms) + count(table.skyStates));
}

void EventResources::Rephase(const assets::EventTrigger& trigger, ResourcePhase from,
                             ResourcePhase to, TrackEvents& events) {
    FireSubEvent(SubEventOf(trigger.subEvents, to), EventEdge::Start, events);
    FireSubEvent(SubEventOf(trigger.subEvents, from), EventEdge::End, events);
}

void EventResources::FireSubEvent(assets::EventId id, EventEdge edge, TrackEvents& events) {
    if (id == assets::kNoEvent) {
        return;
    }
    if (subEventsFired_ == subEventBudget_) {
        if (!hasWarnedOfCascade_) {
            log::Warn("track event {} keeps firing sub-events, dropping the rest this tick", id);
            hasWarnedOfCascade_ = true;
        }
        return;
    }
    ++subEventsFired_;
    events.Fire({.id = id, .edge = edge});
}

void EventResources::Dispatch(TrackEvents& events, TrackEntities& entities) {
    while (dispatched_ < events.Events().size()) {
        const TrackEvent event = events.Events()[dispatched_++];
        if (!event.racer) {
            Apply(event, events, entities);
        }
    }
}

void EventResources::Apply(const TrackEvent& event, TrackEvents& events, TrackEntities& entities) {
    const bool isStart = event.edge == EventEdge::Start;
    const auto visit = [&](const assets::EventTrigger& trigger, ResourcePhase phase, auto start,
                           auto end) {
        if (trigger.event != event.id) {
            return;
        }
        if (isStart) {
            OnStart(trigger, phase, start);
        } else {
            OnEnd(trigger, phase, start, end);
        }
    };
    for (std::size_t i = 0; i < parts_.size(); ++i) {
        if (partEntities_[i]) {
            visit(
                table_->partAnimations[i].trigger, parts_[i], [&] { StartPart(i, entities); },
                [&] { EndPart(i, entities); });
        }
    }
    for (std::size_t i = 0; i < skyStates_.size(); ++i) {
        visit(
            table_->skyStates[i].trigger, skyStates_[i], [&] { StartSky(i); },
            [&] { EndSky(i, events); });
    }
    for (std::size_t i = 0; i < timers_.size(); ++i) {
        visit(
            table_->timers[i].trigger, timers_[i].phase, [&] { StartTimer(i, events); },
            [&] { EndTimer(i, events); });
        if (table_->timers[i].holdEvent == event.id) {
            timers_[i].isHeld = isStart;
        }
    }
    for (std::size_t i = 0; i < nodes_.size(); ++i) {
        visit(
            table_->nodeTransforms[i].trigger, nodes_[i], [&] { StartNode(i, events); },
            [&] { EndNode(i, events); });
    }
    for (std::size_t i = 0; i < distances_.size(); ++i) {
        visit(
            table_->modelDistances[i].trigger, distances_[i],
            [&] { distances_[i] = ResourcePhase::Active; },
            [&] { distances_[i] = ResourcePhase::Idle; });
    }
    for (const assets::EventLink& link : table_->links) {
        if (isStart && link.event == event.id) {
            Force(link.forced);
        }
    }
}

void EventResources::Force(assets::EventId id) {
    const auto force = [id](const auto& resources, auto phaseOf) {
        for (std::size_t i = 0; i < resources.size(); ++i) {
            if (resources[i].trigger.event == id) {
                phaseOf(i) = ResourcePhase::EndPending;
            }
        }
    };
    force(table_->partAnimations, [this](std::size_t i) -> ResourcePhase& { return parts_[i]; });
    force(table_->timers, [this](std::size_t i) -> ResourcePhase& { return timers_[i].phase; });
    force(table_->nodeTransforms, [this](std::size_t i) -> ResourcePhase& { return nodes_[i]; });
    force(table_->modelDistances,
          [this](std::size_t i) -> ResourcePhase& { return distances_[i]; });
    force(table_->skyStates, [this](std::size_t i) -> ResourcePhase& { return skyStates_[i]; });
}

void EventResources::Update(Milliseconds elapsed, TrackEvents& events, TrackEntities& entities) {
    for (std::size_t i = 0; i < parts_.size(); ++i) {
        if (partEntities_[i]) {
            UpdatePart(i, events, entities);
        }
    }
    for (std::size_t i = 0; i < timers_.size(); ++i) {
        UpdateTimer(i, elapsed, events);
    }
    for (std::size_t i = 0; i < nodes_.size(); ++i) {
        UpdateNode(i, events, entities);
    }
    for (ResourcePhase& phase : distances_) {
        if (phase == ResourcePhase::EndPending) {
            phase = ResourcePhase::Idle;
        }
    }
    for (std::size_t i = 0; i < skyStates_.size(); ++i) {
        if (skyStates_[i] == ResourcePhase::EndPending) {
            EndSky(i, events);
        }
    }
    sky_.Tick(elapsed);
}

bool EventResources::IsModelHidden(std::size_t distance) const {
    const bool isApplied = !CanStart(distances_[distance]);
    return isApplied == table_->modelDistances[distance].hidesWhileActive;
}

void EventResources::StartPart(std::size_t i, TrackEntities& entities) {
    const assets::PartAnimationResource& resource = table_->partAnimations[i];
    if (resource.startPart >= 0) {
        entities.Queue(*partEntities_[i], static_cast<std::size_t>(resource.startPart), false);
    } else {
        entities.Queue(*partEntities_[i], static_cast<std::size_t>(resource.activePart), true);
    }
}

void EventResources::EndPart(std::size_t i, TrackEntities& entities) {
    const assets::PartAnimationResource& resource = table_->partAnimations[i];
    const std::size_t entity = *partEntities_[i];
    const auto current = static_cast<std::int32_t>(entities.CurrentPart(entity).value_or(-1));
    if (parts_[i] != ResourcePhase::Idle && current != resource.idlePart) {
        if (resource.endPart < 0) {
            entities.Queue(entity, static_cast<std::size_t>(resource.idlePart), true);
        } else if (current != resource.endPart) {
            entities.Queue(entity, static_cast<std::size_t>(resource.endPart), false);
        }
    }
    parts_[i] = ResourcePhase::Idle;
}

void EventResources::UpdatePart(std::size_t i, TrackEvents& events, TrackEntities& entities) {
    if (parts_[i] == ResourcePhase::EndPending) {
        EndPart(i, entities);
    }
    const assets::PartAnimationResource& resource = table_->partAnimations[i];
    const std::size_t entity = *partEntities_[i];
    const std::optional<std::size_t> part = entities.CurrentPart(entity);
    if (!part) {
        return;
    }
    const auto current = static_cast<std::int32_t>(*part);
    ResourcePhase& phase = parts_[i];
    if (!entities.HasQueued(entity)) {
        if (phase == ResourcePhase::Starting && current == resource.startPart) {
            entities.Queue(entity, static_cast<std::size_t>(resource.activePart),
                           resource.isLooping);
        } else if (phase == ResourcePhase::Ending && current == resource.endPart) {
            entities.Queue(entity, static_cast<std::size_t>(resource.idlePart), true);
        } else if (phase == ResourcePhase::Active && !resource.isLooping &&
                   current == resource.activePart) {
            const bool hasEnd = resource.endPart >= 0;
            entities.Queue(entity,
                           static_cast<std::size_t>(hasEnd ? resource.endPart : resource.idlePart),
                           !hasEnd);
        }
    }
    const auto enter = [&](ResourcePhase to) {
        Rephase(resource.trigger, phase, to, events);
        phase = to;
    };
    if (current == resource.startPart && phase != ResourcePhase::Starting) {
        enter(ResourcePhase::Starting);
    } else if (current == resource.activePart && phase != ResourcePhase::Active) {
        enter(ResourcePhase::Active);
    } else if (current == resource.endPart && phase != ResourcePhase::Ending) {
        enter(ResourcePhase::Ending);
    } else if (current == resource.idlePart && phase != ResourcePhase::Idle) {
        enter(ResourcePhase::Idle);
    }
}

void EventResources::StartTimer(std::size_t i, TrackEvents& events) {
    TimerState& timer = timers_[i];
    timer.left = table_->timers[i].duration;
    Rephase(table_->timers[i].trigger, ResourcePhase::Idle, ResourcePhase::Active, events);
    timer.phase = ResourcePhase::Active;
}

void EventResources::EndTimer(std::size_t i, TrackEvents& events) {
    TimerState& timer = timers_[i];
    if (timer.left == Milliseconds{0} && !timer.isHeld) {
        Rephase(table_->timers[i].trigger, timer.phase, ResourcePhase::Idle, events);
        timer.phase = ResourcePhase::Ending;
    }
}

void EventResources::UpdateTimer(std::size_t i, Milliseconds elapsed, TrackEvents& events) {
    TimerState& timer = timers_[i];
    if (timer.phase == ResourcePhase::EndPending) {
        EndTimer(i, events);
    }
    if (timer.phase == ResourcePhase::Ending) {
        return;
    }
    timer.left = std::max(timer.left - elapsed, Milliseconds{0});
    if (timer.left == Milliseconds{0}) {
        EndTimer(i, events);
    }
}

void EventResources::StartSky(std::size_t i) {
    sky_.Apply(table_->skyStates[i]);
    skyStates_[i] = ResourcePhase::EndPending;
}

void EventResources::EndSky(std::size_t i, TrackEvents& events) {
    Rephase(table_->skyStates[i].trigger, skyStates_[i], ResourcePhase::Active, events);
    skyStates_[i] = ResourcePhase::Idle;
}

void EventResources::StartNode(std::size_t i, TrackEvents& events) {
    Rephase(table_->nodeTransforms[i].trigger, ResourcePhase::Idle, ResourcePhase::Active, events);
    nodes_[i] = ResourcePhase::Active;
}

void EventResources::EndNode(std::size_t i, TrackEvents& events) {
    Rephase(table_->nodeTransforms[i].trigger, nodes_[i], ResourcePhase::Idle, events);
    nodes_[i] = ResourcePhase::Idle;
}

void EventResources::UpdateNode(std::size_t i, TrackEvents& events, const TrackEntities& entities) {
    if (nodes_[i] == ResourcePhase::Idle) {
        return;
    }
    if (nodes_[i] == ResourcePhase::EndPending) {
        EndNode(i, events);
    }
    const NodeTarget& target = nodeTargets_[i];
    if (target.entity && target.world) {
        const Transform node = entities.NodeWorld(
            *target.entity, static_cast<std::size_t>(table_->nodeTransforms[i].node));
        moved_[*target.world] =
            Placement{.position = node.position, .basis = ToBasis(node.rotation)};
    }
}

}
