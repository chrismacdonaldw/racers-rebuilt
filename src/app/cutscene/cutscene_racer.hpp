#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "app/cutscene/cutscene_bindings.hpp"
#include "assets/cutscene_effects.hpp"
#include "assets/cutscene_timeline.hpp"

namespace rr::app {

/// Unbinds the model events on `swap` and `pleg` unless `legs` is the peg leg.
void DropLegVariants(const assets::CutsceneFrame& frame, std::span<ModelBinding> models,
                     std::int32_t legs);

/// The `wincar` texts naming every champion but `champion`, a driver code such as `CR`.
[[nodiscard]] std::vector<std::string> OtherChampionNames(std::string_view champion);

/// Drops the links that start a text named in `names`, so those texts never show.
void HideTexts(assets::CutsceneEffectsAsset& effects, std::span<const std::string> names);

/// 1 to 5 for `angry`, `blink`, `happy`, `sad` and `suprz`, 0 for `dflt` and anything else.
[[nodiscard]] std::int32_t FaceExpression(std::string_view material);

}
