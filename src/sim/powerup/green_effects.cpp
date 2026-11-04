#include "sim/powerup/green_effects.hpp"

#include <cstddef>
#include <memory>

#include "sim/powerup/turbo_effect.hpp"
#include "sim/powerup/warp_effect.hpp"

namespace rr::sim {

void AddGreenEffects(EffectFactories& factories, const WarpRules& warp) {
    factories.at(static_cast<std::size_t>(PowerupKind::Turbo)) = [] {
        return std::make_unique<TurboEffect>();
    };
    factories.at(static_cast<std::size_t>(PowerupKind::Warp)) = [warp] {
        return std::make_unique<WarpEffect>(warp);
    };
}

}
