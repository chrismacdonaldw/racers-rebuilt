#include "sim/powerup/every_effect.hpp"

#include "sim/powerup/green_effects.hpp"
#include "sim/powerup/red_effects.hpp"
#include "sim/powerup/shield_effect.hpp"
#include "sim/powerup/yellow_effects.hpp"

namespace rr::sim {

EffectFactories EveryEffect() {
    EffectFactories factories;
    AddGreenEffects(factories);
    AddBlueEffects(factories);
    AddRedEffects(factories);
    AddYellowEffects(factories);
    return factories;
}

}
