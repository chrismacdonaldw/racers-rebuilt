#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

#include "app/cutscene/cutscene_bindings.hpp"
#include "app/cutscene/cutscene_materials.hpp"
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

/// Renames the keys of tracks any frame binds to `guy1` once, to those expressions of `face`.
void ShowDriverFaces(std::span<const assets::CutsceneFrame> frames, std::string_view face,
                     WorldAnimations& animations);

}
