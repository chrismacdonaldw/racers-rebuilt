#include "app/track_entity_draws.hpp"

#include "app/track_space.hpp"
#include "assets/game_path.hpp"

namespace rr::app {

TrackEntityDraws::TrackEntityDraws(const assets::NameMap<JointedModel>& models,
                                   const sim::TrackEntitySources& sources)
    : sources_(&sources), spans_(sources.entities.size()) {
    for (std::size_t entity = 0; entity < sources.entities.size(); ++entity) {
        const auto model = models.find(assets::ToUpperAscii(sources.entities[entity].model));
        if (model == models.end()) {
            continue;
        }
        const JointedModel& jointed = model->second;
        spans_[entity] = {.first = draws_.size(), .count = jointed.draws.size()};
        for (std::size_t i = 0; i < jointed.draws.size(); ++i) {
            draws_.push_back(jointed.draws[i]);
            slots_.push_back(jointed.slots[i]);
            parts_.push_back({.entity = entity, .joint = jointed.joints[i]});
        }
    }
}

void TrackEntityDraws::Update(std::span<const sim::EntityClock> clocks) {
    for (std::size_t i = 0; i < draws_.size(); ++i) {
        const Part& part = parts_[i];
        const sim::TrackEntity& entity = sources_->entities[part.entity];
        const bool hasJoint = part.joint < sources_->skeletons[entity.skeleton].nodes.size();
        const sim::EntityClock clock =
            part.entity < clocks.size() ? clocks[part.entity] : sim::EntityClock{};
        const Transform placed =
            hasJoint ? sim::NodeWorld(*sources_, part.entity, clock, part.joint) : entity.placement;
        draws_[i].transform = TransformToEngine(placed);
    }
}

}
