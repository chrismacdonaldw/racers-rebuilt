#include "sim/track/track_entities.hpp"

#include <algorithm>

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

Transform NodeWorld(const TrackEntitySources& sources, std::size_t entity, const EntityClock& clock,
                    std::size_t node) {
    const TrackEntity& placed = sources.entities[entity];
    const std::vector<assets::SkeletonNode>& nodes = sources.skeletons[placed.skeleton].nodes;
    Transform pose = LocalAt(sources, placed, clock, node);
    for (std::optional<std::size_t> parent = nodes[node].parent; parent;
         parent = nodes[*parent].parent) {
        pose = Compose(LocalAt(sources, placed, clock, *parent), pose);
    }
    return Compose(placed.placement, pose);
}

TrackEntities::TrackEntities(const TrackEntitySources& sources) : sources_(&sources) {
    clocks_.reserve(sources.entities.size());
    for (const TrackEntity& entity : sources.entities) {
        clocks_.push_back(entity.startPart ? EntityClock{PartClock{.part = *entity.startPart}}
                                           : std::nullopt);
    }
}

void TrackEntities::Tick(Milliseconds elapsed) {
    const auto elapsedMs = static_cast<float>(elapsed.count());
    for (std::size_t i = 0; i < clocks_.size(); ++i) {
        if (clocks_[i]) {
            AdvanceClock(sources_->animations[sources_->entities[i].animation], *clocks_[i],
                         elapsedMs);
        }
    }
}

std::optional<std::size_t> TrackEntities::Find(std::string_view name) const {
    const auto entity = std::ranges::find(sources_->entities, name, &TrackEntity::name);
    if (entity == sources_->entities.end()) {
        return std::nullopt;
    }
    return static_cast<std::size_t>(entity - sources_->entities.begin());
}

Transform TrackEntities::NodeWorld(std::size_t entity, std::size_t node) const {
    return sim::NodeWorld(*sources_, entity, clocks_[entity], node);
}

}
