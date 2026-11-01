#include "app/track_entity_draws.hpp"

#include "app/track_space.hpp"
#include "assets/game_path.hpp"

namespace rr::app {

TrackEntityDraws::TrackEntityDraws(const assets::NameMap<JointedModel>& models,
                                   const sim::TrackEntitySources& sources)
    : sources_(&sources) {
    for (std::size_t entity = 0; entity < sources.entities.size(); ++entity) {
        const auto model = models.find(assets::ToUpperAscii(sources.entities[entity].model));
        if (model == models.end()) {
            continue;
        }
        draws_.insert(draws_.end(), model->second.draws.begin(), model->second.draws.end());
        for (const std::uint16_t joint : model->second.joints) {
            parts_.push_back({.entity = entity, .joint = joint});
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
