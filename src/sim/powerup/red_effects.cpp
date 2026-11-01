#include "sim/powerup/red_effects.hpp"

#include <cstddef>
#include <memory>

#include "sim/powerup/cannonball_effect.hpp"

namespace rr::sim {

void AddRedEffects(EffectFactories& factories) {
    factories.at(static_cast<std::size_t>(PowerupKind::Cannonball)) = [] {
        return std::make_unique<CannonballEffect>();
    };
}

}
