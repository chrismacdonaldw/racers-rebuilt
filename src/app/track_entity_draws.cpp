#include "app/track_entity_draws.hpp"

#include <cmath>

#include "app/texture_scroll.hpp"
#include "app/track_space.hpp"
#include "assets/game_path.hpp"

namespace rr::app {

TrackEntityDraws::TrackEntityDraws(const assets::NameMap<JointedModel>& models,
                                   const sim::TrackEntitySources& sources)
    : sources_(&sources), spans_(sources.entities.size()), poses_(sources.entities.size()) {
    for (std::size_t entity = 0; entity < sources.entities.size(); ++entity) {
        const auto model = models.find(assets::ToUpperAscii(sources.entities[entity].model));
        if (model == models.end()) {
            continue;
        }
        const JointedModel& jointed = model->second;
        spans_[entity] = {.first = draws_.size(), .count = jointed.draws.size()};
        if (const auto& scroll = sources.entities[entity].textureScroll) {
            scrolled_.push_back({.draws = spans_[entity], .scroll = *scroll});
        }
        draws_.insert(draws_.end(), jointed.draws.begin(), jointed.draws.end());
        slots_.insert(slots_.end(), jointed.slots.begin(), jointed.slots.end());
    }
}

void TrackEntityDraws::Update(std::span<const sim::EntityClock> clocks) {
    for (std::size_t entity = 0; entity < spans_.size(); ++entity) {
        const DrawSpan span = spans_[entity];
        if (span.count == 0) {
            continue;
        }
        const sim::EntityClock clock = entity < clocks.size() ? clocks[entity] : sim::EntityClock{};
        std::vector<Transform>& poses = poses_[entity];
        sim::NodePoses(*sources_, entity, clock, poses);
        for (Transform& pose : poses) {
            pose = TransformToEngine(pose);
        }
        const Transform placement = TransformToEngine(sources_->entities[entity].placement);
        for (std::size_t i = span.first; i < span.first + span.count; ++i) {
            draws_[i].transform = placement;
            draws_[i].joints = poses;
        }
    }
}

void TrackEntityDraws::Scroll(float elapsedMs) {
    ScrollDraws(scrolled_, elapsedMs, draws_);
}

void TrackEntityDraws::AddFarModels(std::size_t firstDraw, FarModels& out) const {
    for (std::size_t entity = 0; entity < spans_.size(); ++entity) {
        const sim::TrackEntity& source = sources_->entities[entity];
        if (spans_[entity].count == 0 || !std::isfinite(source.drawRangeSquared)) {
            continue;
        }
        out.Keep(FarKind::Scenery, {.position = ToEngine(source.placement.position),
                                    .draws = {.first = firstDraw + spans_[entity].first,
                                              .count = spans_[entity].count},
                                    .rangeSquared = source.drawRangeSquared});
    }
}

}
