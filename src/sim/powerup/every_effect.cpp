#include "sim/powerup/every_effect.hpp"

#include "sim/powerup/green_effects.hpp"
#include "sim/powerup/red_effects.hpp"
#include "sim/powerup/shield_effect.hpp"
#include "sim/powerup/yellow_effects.hpp"

namespace rr::sim {

EffectFactories EveryEffect(const WarpRules& warp) {
    EffectFactories factories;
    AddGreenEffects(factories, warp);
    AddBlueEffects(factories);
    AddRedEffects(factories);
    AddYellowEffects(factories);
    return factories;
}

}
