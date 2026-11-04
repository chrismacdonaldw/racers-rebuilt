#pragma once

#include <vector>

#include "render/fx_effect.hpp"

namespace rr::app {

/// Modern renderings of the track bank's hazard and weather emitters.
[[nodiscard]] std::vector<render::FxEffect> HazardFxEffects();

}
