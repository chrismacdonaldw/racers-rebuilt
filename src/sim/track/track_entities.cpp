#include "sim/track/track_entities.hpp"

#include <algorithm>
#include <utility>

namespace rr::sim {
namespace {

Transform LocalAt(const TrackEntitySources& sources, const TrackEntity& entity,
                  const EntityClock& clock, std::size_t node) {
    const assets::SkeletonNode& rest = sources.skeletons[entity.skeleton].nodes[node];
    Transform local{.position = rest.position, .rotation = rest.rotation};
    if (clock) {
        SampleNode(sources.animations[entity.animation], *clock, node, local);
    }
    local.position = local.position * entity.scale;
    return local;
}

}

Transform NodeWorldBefore(const TrackEntitySources& sources, std::size_t entity, EntityClock clock,
                          std::size_t node, Milliseconds ago) {
    if (clock) {
        RewindClock(sources.animations[sources.entities[entity].animation], *clock,
                    static_cast<float>(ago.count()));
    }
    return NodeWorld(sources, entity, clock, node);
}

Transform NodeWorld(const TrackEntitySources& sources, std::size_t entity, const EntityClock& clock,
                    std::size_t node) {
    const TrackEntity& placed = sources.entities[entity];
    const std::vector<assets::SkeletonNode>& nodes = sources.skeletons[placed.skeleton].nodes;
    if (node >= nodes.size()) {
        return placed.placement;
    }
    Transform pose = LocalAt(sources, placed, clock, node);
    for (std::optional<std::size_t> parent = nodes[node].parent; parent;
         parent = nodes[*parent].parent) {
        pose = Compose(LocalAt(sources, placed, clock, *parent), pose);
    }
    return Compose(placed.placement, pose);
}

void NodePoses(const TrackEntitySources& sources, std::size_t entity, const EntityClock& clock,
               std::vector<Transform>& out) {
    const TrackEntity& placed = sources.entities[entity];
    const std::vector<assets::SkeletonNode>& nodes = sources.skeletons[placed.skeleton].nodes;
    out.resize(nodes.size());
    for (std::size_t node = 0; node < nodes.size(); ++node) {
        const Transform local = LocalAt(sources, placed, clock, node);
        const std::optional<std::size_t> parent = nodes[node].parent;
        out[node] = parent && *parent < node ? Compose(out[*parent], local) : local;
    }
}

TrackEntities::TrackEntities(const TrackEntitySources& sources) : sources_(&sources) {
    clocks_.reserve(sources.entities.size());
    for (const TrackEntity& entity : sources.entities) {
        clocks_.push_back(entity.startPart ? EntityClock{PartClock{.part = *entity.startPart}}
                                           : std::nullopt);
    }
    queued_.resize(clocks_.size());
    isFrozen_.resize(clocks_.size());
}

void TrackEntities::Tick(Milliseconds elapsed) {
    const auto elapsedMs = static_cast<float>(elapsed.count());
    for (std::size_t i = 0; i < clocks_.size(); ++i) {
        if (!clocks_[i] || isFrozen_[i]) {
            continue;
        }
        const assets::AnimationAsset& animation =
            sources_->animations[sources_->entities[i].animation];
        if (!queued_[i]) {
            AdvanceClock(animation, *clocks_[i], elapsedMs);
            continue;
        }
        PartClock& clock = *clocks_[i];
        PartClock once = clock;
        once.loops = false;
        AdvanceClock(animation, once, elapsedMs);
        if (sim::IsHoldingLastFrame(animation, once)) {
            clocks_[i] = std::exchange(queued_[i], std::nullopt);
        } else {
            clock.frame = once.frame;
        }
    }
}

void TrackEntities::Seek(std::size_t entity, float frame) {
    if (EntityClock& clock = clocks_.at(entity)) {
        clock->frame = frame;
    }
}

float TrackEntities::Frame(std::size_t entity) const {
    const EntityClock& clock = clocks_.at(entity);
    return clock ? clock->frame : 0.0F;
}

void TrackEntities::Queue(std::size_t entity, std::size_t part, bool loops) {
    const PartClock clock{.part = part, .loops = loops};
    if (clocks_[entity]) {
        queued_[entity] = clock;
    } else {
        clocks_[entity] = clock;
    }
}

void TrackEntities::Freeze(std::size_t entity) {
    isFrozen_.at(entity) = true;
}

void TrackEntities::Play(std::size_t entity, std::size_t part, bool loops) {
    clocks_.at(entity) = PartClock{.part = part, .loops = loops};
    queued_[entity].reset();
    isFrozen_[entity] = false;
}

void TrackEntities::PlayOut(std::size_t entity) {
    if (EntityClock& clock = clocks_.at(entity)) {
        clock->loops = false;
    }
    isFrozen_[entity] = false;
}

bool TrackEntities::IsHoldingLastFrame(std::size_t entity) const {
    const EntityClock& clock = clocks_.at(entity);
    return !clock || sim::IsHoldingLastFrame(
                         sources_->animations[sources_->entities[entity].animation], *clock);
}

std::optional<std::size_t> TrackEntities::CurrentPart(std::size_t entity) const {
    return clocks_[entity].transform([](const PartClock& clock) { return clock.part; });
}

std::optional<std::size_t> TrackEntities::Find(std::string_view name) const {
    const auto entity = std::ranges::find(sources_->entities, name, &TrackEntity::name);
    if (entity == sources_->entities.end()) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(entity - sources_->entities.begin());
}

std::optional<Transform> TrackEntities::FindStatic(std::string_view name) const {
    const auto entity = std::ranges::find(sources_->statics, name, &StaticEntity::name);
    if (entity == sources_->statics.end()) {
        return std::nullopt;
    }
    return entity->placement;
}

Transform TrackEntities::NodeWorld(std::size_t entity, std::size_t node) const {
    return sim::NodeWorld(*sources_, entity, clocks_[entity], node);
}

Transform TrackEntities::NodeWorldBefore(std::size_t entity, std::size_t node,
                                         Milliseconds ago) const {
    return sim::NodeWorldBefore(*sources_, entity, clocks_[entity], node, ago);
}

}
