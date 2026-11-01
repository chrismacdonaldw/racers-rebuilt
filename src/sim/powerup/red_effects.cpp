#include "sim/powerup/red_effects.hpp"

#include <cstddef>
#include <memory>

#include "sim/powerup/cannonball_effect.hpp"
#include "sim/powerup/grapple_effect.hpp"
#include "sim/powerup/lightning_effect.hpp"
#include "sim/powerup/missile_effect.hpp"

namespace rr::sim {

void AddRedEffects(EffectFactories& factories) {
    factories.at(static_cast<std::size_t>(PowerupKind::Cannonball)) = [] {
        return std::make_unique<CannonballEffect>();
    };
    factories.at(static_cast<std::size_t>(PowerupKind::GrapplingHook)) = [] {
        return std::make_unique<GrappleEffect>();
    };
    factories.at(static_cast<std::size_t>(PowerupKind::Lightning)) = [] {
        return std::make_unique<LightningEffect>();
    };
    factories.at(static_cast<std::size_t>(PowerupKind::Missile)) = [] {
        return std::make_unique<MissileEffect>();
    };
}

}
