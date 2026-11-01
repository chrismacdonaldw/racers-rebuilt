#include "app/hidden_models.hpp"

#include <string>

#include "assets/game_path.hpp"

namespace rr::app {

HiddenModels::HiddenModels(std::span<const assets::ModelDistanceResource> distances,
                           std::span<const std::string> switches,
                           std::span<const NamedDraws> trackDraws,
                           const sim::TrackEntitySources& sources) {
    for (const assets::ModelDistanceResource& distance : distances) {
        Add(distance.entity ? std::string_view(distance.entity->name) : std::string_view{},
            trackDraws, sources);
    }
    for (const std::string& entity : switches) {
        Add(entity, trackDraws, sources);
    }
}

void HiddenModels::Apply(const std::vector<bool>& hidden, std::span<const render::MeshDraw> track,
                         const TrackEntityDraws& entities, std::size_t entityOffset,
                         std::span<render::MeshDraw> draws) const {
    for (std::size_t i = 0; i < targets_.size() && i < hidden.size(); ++i) {
        for (const DrawSpan& span : targets_[i].track) {
            for (std::size_t draw = span.first; draw < span.first + span.count; ++draw) {
                draws[draw].indexCount = hidden[i] ? 0 : track[draw].indexCount;
            }
        }
        for (const std::size_t entity : targets_[i].entities) {
            const DrawSpan span = entities.SpanOf(entity);
            for (std::size_t draw = 0; hidden[i] && draw < span.count; ++draw) {
                draws[entityOffset + span.first + draw].indexCount = 0;
            }
        }
    }
}

void HiddenModels::Add(std::string_view entity, std::span<const NamedDraws> trackDraws,
                       const sim::TrackEntitySources& sources) {
    Target& target = targets_.emplace_back();
    if (entity.empty()) {
        return;
    }
    const std::string name = assets::ToUpperAscii(entity);
    for (const NamedDraws& named : trackDraws) {
        if (assets::ToUpperAscii(named.entity) == name) {
            target.track.push_back(named.span);
        }
    }
    for (std::size_t i = 0; i < sources.entities.size(); ++i) {
        if (assets::ToUpperAscii(sources.entities[i].name) == name) {
            target.entities.push_back(i);
        }
    }
}

}
