#include "app/hazard/hazard_materials.hpp"

#include <algorithm>
#include <array>

namespace rr::app {
namespace {

constexpr std::string_view kLampPrefix = "mmcode";
constexpr std::string_view kSphinx = "blowup";
constexpr std::size_t kSphinxLoops = 2;
constexpr std::array<std::int64_t, sim::kCodeGates> kLampFramesPerSecond{3, 4, 5};
constexpr std::int64_t kMillisecondsPerSecond = 1'000;

std::int64_t LastFrame(const PlayedTrack& played) {
    return played.frames.tracks.front().duration - 1;
}

std::int64_t FramesIn(std::int64_t framesPerSecond, Milliseconds played) {
    return framesPerSecond * played.count() / kMillisecondsPerSecond;
}

const render::Material& KeyShown(const PlayedTrack& played, std::int64_t frames) {
    return played.materials[assets::KeyAtFrame(played.frames, 0, frames)];
}

}

std::optional<HazardDriven> FindHazardDriven(std::string_view entity, std::size_t track) {
    if (entity == kSphinx && track < kSphinxLoops) {
        return HazardDriven{.kind = HazardDriven::Kind::Sphinx,
                            .blowUpTrack = track + kSphinxLoops};
    }
    if (entity.size() != kLampPrefix.size() + 1 || !entity.starts_with(kLampPrefix)) {
        return std::nullopt;
    }
    const auto lamp = static_cast<std::size_t>(entity.back() - '1');
    if (lamp >= sim::kCodeGates) {
        return std::nullopt;
    }
    return HazardDriven{.kind = HazardDriven::Kind::Lamp, .lamp = lamp};
}

const render::Material* HazardShown(const HazardDriven& driven, const PlayedTrack& bound,
                                    const sim::HazardMaterialsSnapshot& hazards) {
    if (driven.kind == HazardDriven::Kind::Sphinx) {
        if (!hazards.sphinxBlowingUp) {
            return nullptr;
        }
        const std::int64_t frames =
            FramesIn(driven.blowUp.frames.tracks.front().framesPerSecond, *hazards.sphinxBlowingUp);
        return &KeyShown(driven.blowUp, std::min(frames, LastFrame(driven.blowUp)));
    }
    if (!hazards.areLampsRed) {
        return nullptr;
    }
    const std::int64_t held = hazards.areLampsRed->at(driven.lamp) ? 0 : LastFrame(bound);
    const std::int64_t flashed =
        hazards.lampsFlashing
            ? FramesIn(kLampFramesPerSecond.at(driven.lamp), *hazards.lampsFlashing)
            : 0;
    return &KeyShown(bound, held + flashed);
}

}
