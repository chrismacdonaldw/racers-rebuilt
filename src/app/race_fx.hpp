#pragma once

#include <vector>

#include "render/fx_effect.hpp"

namespace rr::app {

/// The race's better particle effects, each replacing the `EMITTER.EMB` emitter it names.
[[nodiscard]] std::vector<render::FxEffect> RaceFxEffects();

}
