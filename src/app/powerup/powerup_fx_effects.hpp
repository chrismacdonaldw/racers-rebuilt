#pragma once

#include <vector>

#include "render/fx_effect.hpp"

namespace rr::app {

/// Modern renderings of the original lightning strike and oil bubble emitters.
[[nodiscard]] std::vector<render::FxEffect> PowerupFxEffects();

}
