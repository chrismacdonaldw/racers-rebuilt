#include "sim/powerup/yellow_effects.hpp"

#include <cstddef>
#include <memory>

#include "sim/powerup/curse_effect.hpp"
#include "sim/powerup/dynamite_effect.hpp"
#include "sim/powerup/magnet_effect.hpp"
#include "sim/powerup/oil_slick_effect.hpp"

namespace rr::sim {

void AddYellowEffects(EffectFactories& factories) {
    factories.at(static_cast<std::size_t>(PowerupKind::OilSlick)) = [] {
        return std::make_unique<OilSlickEffect>();
    };
    factories.at(static_cast<std::size_t>(PowerupKind::Dynamite)) = [] {
        return std::make_unique<DynamiteEffect>();
    };
    factories.at(static_cast<std::size_t>(PowerupKind::Magnet)) = [] {
        return std::make_unique<MagnetEffect>();
    };
    factories.at(static_cast<std::size_t>(PowerupKind::Curse)) = [] {
        return std::make_unique<CurseEffect>();
    };
}

}
