#include "app/cutscene/cutscene_tints.hpp"

#include <algorithm>
#include <format>
#include <utility>

#include "app/racer/car_look.hpp"

namespace rr::app {
namespace {

constexpr std::int32_t kMaxShift = 8;

render::VertexColorTransform ColorsOf(const assets::CutsceneTint& tint) {
    const auto shift = [&](std::size_t channel) {
        return std::clamp(tint.shifts.at(channel), 0, kMaxShift);
    };
    return ToVertexColors({.shifts = {shift(0), shift(1), shift(2), 0},
                           .offsets = {tint.offsets[0], tint.offsets[1], tint.offsets[2], 0}});
}

bool IsSameEntity(const ModelBinding& a, const ModelBinding& b) {
    if (a.staticDraws && b.staticDraws) {
        return a.staticDraws->first == b.staticDraws->first;
    }
    return a.jointed && a.jointed == b.jointed;
}

}

CutsceneTints::CutsceneTints(std::span<const assets::CutsceneTint> tints,
                             std::vector<std::optional<ModelBinding>> targets)
    : targets_(std::move(targets)), startOrder_(tints.size()) {
    colors_.reserve(tints.size());
    for (const assets::CutsceneTint& tint : tints) {
        colors_.push_back(ColorsOf(tint));
    }
}

void CutsceneTints::StopAll() {
    std::ranges::fill(startOrder_, 0);
}

void CutsceneTints::Apply(const ModelBinding& model, std::span<render::MeshDraw> draws) const {
    std::optional<std::size_t> newest;
    for (std::size_t tint = 0; tint < startOrder_.size(); ++tint) {
        const std::optional<ModelBinding>& target = targets_[tint];
        const bool isNewer = !newest || startOrder_[tint] > startOrder_[*newest];
        if (startOrder_[tint] != 0 && target && IsSameEntity(*target, model) && isNewer) {
            newest = tint;
        }
    }
    if (!newest) {
        return;
    }
    for (render::MeshDraw& draw : draws) {
        draw.material.vertexColors = colors_[*newest];
    }
}

Result<std::vector<std::optional<ModelBinding>>> BindTints(
    std::span<const assets::CutsceneTint> tints, std::span<const assets::WorldAsset* const> worlds,
    std::span<const NamedDraws> statics, const sim::TrackEntitySources& entities) {
    std::vector<std::optional<ModelBinding>> targets;
    for (const assets::CutsceneTint& tint : tints) {
        if (!tint.entity) {
            targets.emplace_back();
            continue;
        }
        Result<ModelBinding> bound = BindEntity(*tint.entity, worlds, statics, entities);
        if (!bound) {
            return Fail(bound.error().code,
                        std::format("tint {}: {}", tint.name, bound.error().message));
        }
        targets.emplace_back(*bound);
    }
    return targets;
}

}
