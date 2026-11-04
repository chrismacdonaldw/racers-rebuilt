#pragma once

#include <memory>
#include <string_view>
#include <vector>

#include "render/fx_effect.hpp"
#include "render/fx_system.hpp"
#include "render/renderer.hpp"

namespace rr::app {

/// The race's better particle effects, each replacing the `EMITTER.EMB` emitter it names.
[[nodiscard]] std::vector<render::FxEffect> RaceFxEffects();

/// Sparks where a car scrapes a wall, which the original has no emitter for.
inline constexpr std::string_view kWallSparksEffect = "wallsprk";

/// Blasts have no original emitter, so their effect is burst by name.
inline constexpr std::string_view kBlastEffect = "blast";

/// Null, after logging why, when its textures can't be uploaded.
[[nodiscard]] std::unique_ptr<render::FxSystem> MakeRaceFx(render::Renderer& renderer);

}
