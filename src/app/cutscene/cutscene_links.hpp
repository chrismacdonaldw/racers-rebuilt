#pragma once

#include <cstddef>
#include <span>
#include <vector>

#include "app/cutscene/cutscene_timeline.hpp"
#include "assets/cutscene_effects.hpp"
#include "assets/cutscene_timeline.hpp"
#include "core/vector3.hpp"

namespace rr::app {

/// Placed where the transform that triggered it is.
struct EffectCommand {
    assets::EffectKind kind = assets::EffectKind::Sound;
    std::size_t effect = 0;
    bool starts = true;
    Vector3 position;
    assets::CutsceneAxes axes;

    friend bool operator==(const EffectCommand&, const EffectCommand&) = default;
};

/// Appends the commands each transform begin or end fires, in change order and then link order.
void DispatchLinks(const assets::CutsceneFrame& frame, std::span<const assets::CutsceneLink> links,
                   std::span<const CutsceneEventChange> changes, std::vector<EffectCommand>& out);

}
