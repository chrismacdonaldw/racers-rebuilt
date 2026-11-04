#pragma once

#include <vector>

#include "render/fx_effect.hpp"

namespace rr::app {

/// Replaces the surface table's grass, sand, dirt, water, swamp and snow wheel emitters.
[[nodiscard]] std::vector<render::FxEffect> SurfaceFxEffects();

}
