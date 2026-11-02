#include "app/cutscene/cutscene_materials.hpp"

#include <algorithm>
#include <format>
#include <string>
#include <utility>

#include "assets/game_path.hpp"

namespace rr::app {
namespace {

bool IsSameTrack(const assets::CutsceneMaterialBinding& a,
                 const assets::CutsceneMaterialBinding& b) {
    return a.world == b.world && a.animation == b.animation && a.track == b.track;
}

Result<assets::MaterialAnimationAsset> ReadAnimation(const assets::Vfs& vfs,
                                                     std::string_view folder,
                                                     std::string_view name) {
    const std::string file = std::format("{}/{}.MAB", folder, assets::ToUpperAscii(name));
    Result<assets::MaterialAnimationAsset> animation = vfs.Read(file).and_then(
        [](const assets::FileData& data) { return assets::ParseMaterialAnimation(data.Bytes()); });
    if (!animation) {
        return Fail(animation.error().code, std::format("{}: {}", file, animation.error().message));
    }
    return animation;
}

}

Result<WorldAnimations> LoadWorldAnimations(const assets::Vfs& vfs, std::string_view folder,
                                            std::span<const assets::WorldAsset* const> worlds) {
    WorldAnimations loaded;
    loaded.reserve(worlds.size());
    for (const assets::WorldAsset* world : worlds) {
        std::vector<assets::MaterialAnimationAsset>& animations = loaded.emplace_back();
        for (const std::string& name : world->materialAnimations) {
            Result<assets::MaterialAnimationAsset> animation = ReadAnimation(vfs, folder, name);
            if (!animation) {
                return std::unexpected(std::move(animation.error()));
            }
            animations.push_back(std::move(*animation));
        }
    }
    return loaded;
}

Result<CutsceneMaterials> CutsceneMaterials::Create(
    const assets::CutsceneFrame& frame, std::span<const ModelBinding> models,
    const WorldAnimations& animations, const assets::NameMap<render::Material>& materials) {
    CutsceneMaterials created;
    for (std::size_t model = 0; model < frame.models.size(); ++model) {
        const ModelBinding& binding = models[model];
        std::optional<Entity>& entity = created.entities_.emplace_back();
        if (binding.jointed) {
            entity = Entity{.isJointed = true, .index = *binding.jointed};
        } else if (binding.staticDraws) {
            entity = Entity{.isJointed = false, .index = binding.staticDraws->first};
        }
        std::vector<Use>& uses = created.uses_.emplace_back();
        for (const assets::CutsceneMaterialBinding& material : frame.models[model].materials) {
            if (material.materialTable != 0) {
                continue;
            }
            Result<std::size_t> track = created.TrackFor(material, animations, materials);
            if (!track) {
                return Fail(track.error().code,
                            std::format("{}: {}", frame.models[model].name, track.error().message));
            }
            uses.push_back({.track = *track, .slot = material.slot});
        }
    }
    return created;
}

Result<std::size_t> CutsceneMaterials::TrackFor(
    const assets::CutsceneMaterialBinding& binding, const WorldAnimations& animations,
    const assets::NameMap<render::Material>& materials) {
    const auto existing = std::ranges::find_if(
        trackSources_, [&](const auto& source) { return IsSameTrack(source, binding); });
    if (existing != trackSources_.end()) {
        return static_cast<std::size_t>(existing - trackSources_.begin());
    }
    if (binding.world >= animations.size() ||
        binding.animation >= animations[binding.world].size() ||
        binding.track >= animations[binding.world][binding.animation].tracks.size()) {
        return Fail(ErrorCode::InvalidData,
                    std::format("material animation {} track {} of world {} doesn't exist",
                                binding.animation, binding.track, binding.world));
    }
    const assets::MaterialAnimationAsset& source = animations[binding.world][binding.animation];
    assets::MaterialTrack track = source.tracks[binding.track];
    Track added;
    for (std::size_t key = track.firstKey; key < track.firstKey + track.keyCount; ++key) {
        const std::string& name = source.keys[key].material;
        const auto material = materials.find(assets::ToUpperAscii(name));
        if (material == materials.end()) {
            return Fail(ErrorCode::NotFound, std::format("unable to find material {}", name));
        }
        added.animation.keys.push_back(source.keys[key]);
        added.materials.push_back(material->second);
    }
    track.firstKey = 0;
    added.animation.tracks.push_back(track);
    tracks_.push_back(std::move(added));
    trackSources_.push_back(binding);
    return tracks_.size() - 1;
}

void CutsceneMaterials::Begin(std::size_t model) {
    if (!entities_[model]) {
        return;
    }
    for (const Use& use : uses_[model]) {
        Track& track = tracks_[use.track];
        track.target = Target{.entity = *entities_[model], .slot = use.slot};
        track.owner = model;
    }
}

void CutsceneMaterials::End(std::size_t model) {
    for (const Use& use : uses_[model]) {
        Track& track = tracks_[use.track];
        if (track.owner == model) {
            track.target.reset();
        }
    }
}

void CutsceneMaterials::Advance(Milliseconds elapsed) {
    for (Track& track : tracks_) {
        if (!track.target) {
            continue;
        }
        track.age += elapsed;
        Show(*track.target, track.materials[assets::SampleTrack(track.animation, 0, track.age)]);
    }
}

void CutsceneMaterials::Show(const Target& target, const render::Material& material) {
    const auto shown = std::ranges::find_if(shown_, [&](const Shown& entry) {
        return entry.target.entity == target.entity && entry.target.slot == target.slot;
    });
    if (shown == shown_.end()) {
        shown_.push_back({.target = target, .material = material});
    } else {
        shown->material = material;
    }
}

void CutsceneMaterials::Apply(std::size_t model, std::span<render::MeshDraw> draws,
                              std::span<const std::uint32_t> slots) const {
    if (!entities_[model]) {
        return;
    }
    for (const Shown& shown : shown_) {
        if (shown.target.entity != *entities_[model]) {
            continue;
        }
        for (std::size_t i = 0; i < draws.size(); ++i) {
            if (slots[i] == shown.target.slot) {
                draws[i].material = shown.material;
            }
        }
    }
}

}
