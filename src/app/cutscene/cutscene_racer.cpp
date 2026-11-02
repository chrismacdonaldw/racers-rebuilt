#include "app/cutscene/cutscene_racer.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <format>
#include <set>
#include <tuple>
#include <variant>

#include "assets/driver_parts.hpp"
#include "assets/game_path.hpp"

namespace rr::app {
namespace {

constexpr std::int32_t kPegLeg = 10;
constexpr std::array<std::string_view, 2> kLegVariants{"SWAP", "PLEG"};
constexpr std::string_view kChampionTextPrefix = "text";
constexpr std::array<std::string_view, 6> kChampionCodes{"CR", "KK", "BB", "JT", "GM", "BVB"};
constexpr std::string_view kDriverEntity = "GUY1";

bool IsJointedNamed(const assets::CutsceneModel& event, std::string_view upperName) {
    const auto* entity = std::get_if<assets::EntityByName>(&event.model);
    return entity != nullptr && entity->isJointed &&
           assets::ToUpperAscii(entity->name) == upperName;
}

using TrackKey = std::tuple<std::size_t, std::size_t, std::size_t>;

std::set<TrackKey> DriverTracks(std::span<const assets::CutsceneFrame> frames) {
    std::set<TrackKey> tracks;
    for (const assets::CutsceneFrame& frame : frames) {
        for (const assets::CutsceneModel& event : frame.models) {
            if (!IsJointedNamed(event, kDriverEntity)) {
                continue;
            }
            for (const assets::CutsceneMaterialBinding& binding : event.materials) {
                tracks.emplace(binding.world, binding.animation, binding.track);
            }
        }
    }
    return tracks;
}

void RenameKeys(assets::MaterialAnimationAsset& animation, std::size_t track,
                std::string_view face) {
    if (track >= animation.tracks.size()) {
        return;
    }
    const assets::MaterialTrack& keys = animation.tracks[track];
    const std::size_t end = std::min(keys.firstKey + keys.keyCount, animation.keys.size());
    for (std::size_t key = keys.firstKey; key < end; ++key) {
        std::string& material = animation.keys[key].material;
        material = assets::FaceMaterial(face, assets::FaceExpression(material));
    }
}

bool IsLegVariant(const assets::CutsceneModel& event) {
    return std::ranges::any_of(
        kLegVariants, [&](std::string_view variant) { return IsJointedNamed(event, variant); });
}

}

void DropLegVariants(const assets::CutsceneFrame& frame, std::span<ModelBinding> models,
                     std::int32_t legs) {
    if (legs == kPegLeg) {
        return;
    }
    for (std::size_t event = 0; event < frame.models.size() && event < models.size(); ++event) {
        if (IsLegVariant(frame.models[event])) {
            models[event] = {};
        }
    }
}

std::vector<std::string> OtherChampionNames(std::string_view champion) {
    std::vector<std::string> names;
    for (const std::string_view code : kChampionCodes) {
        if (!champion.starts_with(code)) {
            names.push_back(std::format("{}{}", kChampionTextPrefix, code));
        }
    }
    return names;
}

void HideTexts(assets::CutsceneEffectsAsset& effects, std::span<const std::string> names) {
    const auto isHidden = [&](const assets::CutsceneLink& link) {
        if (link.kind != assets::EffectKind::Text || link.effect >= effects.texts.size()) {
            return false;
        }
        const std::string text = assets::ToUpperAscii(effects.texts[link.effect].name);
        return std::ranges::any_of(
            names, [&](const std::string& name) { return assets::ToUpperAscii(name) == text; });
    };
    std::erase_if(effects.links, isHidden);
}

void ShowDriverFaces(std::span<const assets::CutsceneFrame> frames, std::string_view face,
                     WorldAnimations& animations) {
    for (const auto& [world, animation, track] : DriverTracks(frames)) {
        if (world < animations.size() && animation < animations[world].size()) {
            RenameKeys(animations[world][animation], track, face);
        }
    }
}

}
