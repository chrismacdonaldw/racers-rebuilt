#include "app/powerup/powerup_fx_effects.hpp"

namespace rr::app {
namespace {

using render::BlendMode;
using render::FxEffect;
using render::FxShape;

constexpr FxEffect kStrikeOrbs{.emitter = "lghthit",
                               .shape = FxShape::Glow,
                               .lifeMs = {.min = 0.9F, .max = 1.1F},
                               .startSize = {.min = 1.5F, .max = 1.7F},
                               .endSize = {.min = 1.5F, .max = 1.7F},
                               .inherit = 1.0F,
                               .color = {.r = 0.45F, .g = 0.8F, .b = 1.0F, .a = 0.85F},
                               .fadeInShare = 0.0F,
                               .fadeOutShare = 0.4F,
                               .blend = BlendMode::Additive,
                               .followsEmitter = true};

constexpr FxEffect kStrikeCores{.emitter = "lghthit",
                                .shape = FxShape::Glow,
                                .lifeMs = {.min = 0.6F, .max = 0.8F},
                                .startSize = {.min = 0.5F, .max = 0.6F},
                                .endSize = {.min = 0.5F, .max = 0.6F},
                                .inherit = 1.0F,
                                .color = {.r = 0.9F, .g = 0.95F, .b = 1.0F},
                                .fadeInShare = 0.0F,
                                .fadeOutShare = 0.5F,
                                .blend = BlendMode::Additive,
                                .followsEmitter = true};

constexpr FxEffect kOilBubbles{.emitter = "oilbub",
                               .shape = FxShape::Bubble,
                               .lifeMs = {.min = 1.0F, .max = 1.0F},
                               .startSize = {.min = 0.7F, .max = 0.9F},
                               .endSize = {.min = 0.8F, .max = 1.0F},
                               .inherit = 1.0F,
                               .color = {.a = 0.95F},
                               .fadeInShare = 0.3F,
                               .fadeOutShare = 0.12F,
                               .followsEmitter = true};

}

std::vector<render::FxEffect> PowerupFxEffects() {
    return {kStrikeOrbs, kStrikeCores, kOilBubbles};
}

}
