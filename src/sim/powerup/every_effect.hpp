#pragma once

#include "sim/powerup/powerup_effect.hpp"
#include "sim/powerup/warp_rules.hpp"

namespace rr::sim {

[[nodiscard]] EffectFactories EveryEffect(const WarpRules& warp = {});

}
