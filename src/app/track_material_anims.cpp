#include "app/track_material_anims.hpp"

#include <algorithm>
#include <chrono>
#include <optional>
#include <string_view>
#include <utility>

#include "app/track_entity_draws.hpp"
#include "assets/game_path.hpp"
#include "core/duration.hpp"
#include "core/log.hpp"

namespace rr::app {
namespace {

const assets::MaterialAnimationAsset* AnimationOf(const assets::WorldAsset& world,
                                                  std::size_t animation,
                                                  const MaterialSources& sources) {
    if (animation >= world.materialAnimations.size()) {
        return nullptr;
    }
    const auto found =
        sources.animations->find(assets::ToUpperAscii(world.materialAnimations[animation]));
    return found == sources.animations->end() ? nullptr : &*found->second;
}

std::optional<PlayedTrack> TrackFrames(const assets::MaterialAnimationAsset& source,
                                       std::size_t track, const MaterialSources& sources) {
    if (track >= source.tracks.size()) {
        return std::nullopt;
    }
    PlayedTrack played;
    assets::MaterialTrack cycle = source.tracks[track];
    for (std::size_t key = cycle.firstKey; key < cycle.firstKey + cycle.keyCount; ++key) {
        const auto material =
            sources.materials->find(assets::ToUpperAscii(source.keys[key].material));
        if (material == sources.materials->end()) {
            return std::nullopt;
        }
        played.frames.keys.push_back(source.keys[key]);
        played.materials.push_back(material->second);
    }
    cycle.firstKey = 0;
    played.frames.tracks.push_back(cycle);
    return played;
}

std::optional<HazardDriven> DrivenBy(std::string_view entity, const assets::WorldAsset& world,
                                     std::size_t animation, std::size_t track,
                                     const MaterialSources& sources) {
    std::optional<HazardDriven> driven = FindHazardDriven(entity, track);
    if (driven && driven->kind == HazardDriven::Kind::Sphinx) {
        std::optional<PlayedTrack> blowUp =
            ResolveTrack(world, animation, driven->blowUpTrack, sources);
        if (!blowUp) {
            return std::nullopt;
        }
        driven->blowUp = std::move(*blowUp);
    }
    return driven;
}

}

std::optional<PlayedTrack> ResolveTrack(const assets::WorldAsset& world, std::size_t animation,
                                        std::size_t track, const MaterialSources& sources) {
    const assets::MaterialAnimationAsset* source = AnimationOf(world, animation, sources);
    return source == nullptr ? std::nullopt : TrackFrames(*source, track, sources);
}

void TrackMaterialAnims::Bind(const assets::WorldAsset& world, const assets::WorldEntity& entity,
                              std::span<const std::uint32_t> slots, std::size_t firstDraw,
                              const MaterialSources& sources) {
    for (const assets::MaterialAnimationBinding& binding : entity.materialAnimations) {
        const auto animation = static_cast<std::size_t>(binding.animation);
        const auto track = static_cast<std::size_t>(binding.track);
        Track bound{.world = &world,
                    .animation = animation,
                    .track = track,
                    .entity = entity.name,
                    .slot = static_cast<std::uint32_t>(binding.material)};
        std::optional<PlayedTrack> played = ResolveTrack(world, animation, track, sources);
        if (!played) {
            log::Warn("{} binds material animation {} track {}, which can't be played", entity.name,
                      binding.animation, binding.track);
            continue;
        }
        bound.played = std::move(*played);
        bound.driven = DrivenBy(entity.name, world, animation, track, sources);
        for (std::size_t i = 0; i < slots.size(); ++i) {
            if (slots[i] == bound.slot) {
                bound.trackDraws.push_back(firstDraw + i);
            }
        }
        const auto same = std::ranges::find_if(tracks_, [&](const Track& other) {
            return other.world == &world && other.animation == animation && other.track == track;
        });
        if (same == tracks_.end()) {
            tracks_.push_back(std::move(bound));
        } else {
            *same = std::move(bound);
        }
    }
}

void TrackMaterialAnims::FindEntities(const sim::TrackEntitySources& sources,
                                      const TrackEntityDraws& draws) {
    for (Track& track : tracks_) {
        track.entityDraws.clear();
        if (!track.trackDraws.empty()) {
            continue;
        }
        const auto entity =
            std::ranges::find(sources.entities, track.entity, &sim::TrackEntity::name);
        if (entity == sources.entities.end()) {
            continue;
        }
        const DrawSpan span =
            draws.SpanOf(static_cast<std::size_t>(entity - sources.entities.begin()));
        for (std::size_t draw = span.first; draw < span.first + span.count; ++draw) {
            if (draws.Slots()[draw] == track.slot) {
                track.entityDraws.push_back(draw);
            }
        }
    }
}

void TrackMaterialAnims::Apply(float shownMs, const sim::HazardMaterialsSnapshot& hazards,
                               std::size_t entityFirst, std::span<render::MeshDraw> draws) const {
    const Milliseconds age{static_cast<Milliseconds::rep>(shownMs)};
    for (const Track& track : tracks_) {
        const render::Material* driven =
            track.driven ? HazardShown(*track.driven, track.played, hazards) : nullptr;
        const render::Material& shown =
            driven != nullptr
                ? *driven
                : track.played.materials[assets::SampleTrack(track.played.frames, 0, age)];
        const auto show = [&](std::size_t draw) {
            if (draw < draws.size()) {
                const render::UvOffset scroll = draws[draw].material.uvOffset;
                draws[draw].material = shown;
                draws[draw].material.uvOffset = scroll;
            }
        };
        std::ranges::for_each(track.trackDraws, show);
        for (const std::size_t draw : track.entityDraws) {
            show(entityFirst + draw);
        }
    }
}

}
