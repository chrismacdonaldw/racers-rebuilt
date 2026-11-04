#include "app/hazard/hazard_fx_effects.hpp"

#include <string_view>

namespace rr::app {
namespace {

using render::BlendMode;
using render::Color;
using render::FxEffect;
using render::FxShape;

constexpr Color kWhiteSmoke{.r = 0.96F, .g = 0.96F, .b = 0.95F};
constexpr Color kGreySmoke{.r = 0.62F, .g = 0.6F, .b = 0.58F};
constexpr Color kSandSmoke{.r = 0.82F, .g = 0.77F, .b = 0.68F};
constexpr Color kFlash{.r = 1.0F, .g = 0.88F, .b = 0.55F};
constexpr Color kFlashCore{.r = 1.0F, .g = 0.98F, .b = 0.9F};
constexpr Color kLava{.r = 1.0F, .g = 0.5F, .b = 0.12F};
constexpr Color kLavaCore{.r = 1.0F, .g = 0.88F, .b = 0.45F};
constexpr Color kGhost{.r = 1.0F, .g = 0.18F, .b = 0.12F};
constexpr Color kGhostCore{.r = 1.0F, .g = 0.65F, .b = 0.6F};
constexpr Color kMagic{.r = 0.3F, .g = 0.8F, .b = 1.0F};
constexpr Color kSnow{.r = 0.93F, .g = 0.95F, .b = 1.0F, .a = 0.9F};
constexpr Color kWater{.r = 0.7F, .g = 0.85F, .b = 1.0F, .a = 0.8F};

constexpr FxEffect Puff(std::string_view emitter, Color color, float alpha) {
    return {.emitter = emitter,
            .lifeMs = {.min = 0.85F, .max = 1.15F},
            .startSize = {.min = 0.85F, .max = 1.1F},
            .endSize = {.min = 0.9F, .max = 1.2F},
            .spinPerSecond = {.min = 0.2F, .max = 0.8F},
            .inherit = 1.0F,
            .spread = {.x = 1.0F, .y = 1.0F, .z = 1.0F},
            .drag = 0.5F,
            .color = {.r = color.r, .g = color.g, .b = color.b, .a = alpha},
            .shade = 0.72F,
            .fadeInShare = 0.12F,
            .fadeOutShare = 0.6F,
            .followsEmitter = true};
}

constexpr FxEffect Glow(std::string_view emitter, Color color, float scale) {
    return {.emitter = emitter,
            .shape = FxShape::Glow,
            .lifeMs = {.min = 0.9F, .max = 1.1F},
            .startSize = {.min = scale, .max = scale * 1.15F},
            .endSize = {.min = scale, .max = scale * 1.15F},
            .inherit = 1.0F,
            .color = color,
            .fadeInShare = 0.0F,
            .fadeOutShare = 0.4F,
            .blend = BlendMode::Additive,
            .followsEmitter = true};
}

constexpr FxEffect kMagicStars{.emitter = "wizard",
                               .shape = FxShape::Star,
                               .lifeMs = {.min = 0.8F, .max = 1.0F},
                               .startSize = {.min = 0.7F, .max = 0.9F},
                               .endSize = {.min = 0.7F, .max = 0.9F},
                               .spinPerSecond = {.min = 2.0F, .max = 5.0F},
                               .inherit = 1.0F,
                               .color = {.r = 0.85F, .g = 0.97F, .b = 1.0F},
                               .fadeInShare = 0.0F,
                               .fadeOutShare = 0.4F,
                               .blend = BlendMode::Additive,
                               .followsEmitter = true};

constexpr FxEffect kSnowflakes{.emitter = "snow",
                               .shape = FxShape::Glow,
                               .lifeMs = {.min = 1.0F, .max = 1.0F},
                               .startSize = {.min = 0.6F, .max = 0.8F},
                               .endSize = {.min = 0.6F, .max = 0.8F},
                               .spinPerSecond = {.min = 0.5F, .max = 1.5F},
                               .inherit = 1.0F,
                               .spread = {.x = 1.0F, .z = 1.0F},
                               .color = kSnow,
                               .fadeInShare = 0.2F,
                               .fadeOutShare = 0.3F,
                               .followsEmitter = true};

constexpr FxEffect kSplashDrops{.emitter = "splash",
                                .shape = FxShape::Streak,
                                .lifeMs = {.min = 0.8F, .max = 1.0F},
                                .startSize = {.min = 0.25F, .max = 0.35F},
                                .endSize = {.min = 0.15F, .max = 0.2F},
                                .inherit = 1.0F,
                                .spread = {.x = 4.0F, .y = 3.0F, .z = 4.0F},
                                .streakSeconds = 0.05F,
                                .color = kWater,
                                .fadeInShare = 0.0F,
                                .fadeOutShare = 0.3F,
                                .followsEmitter = true};

}

std::vector<render::FxEffect> HazardFxEffects() {
    return {Glow("canexpl", kFlash, 2.2F),        Glow("canexpl", kFlashCore, 1.0F),
            Puff("cansmoke", kGreySmoke, 0.55F),  Puff("pillrsmk", kSandSmoke, 0.5F),
            Puff("doorsmk", kSandSmoke, 0.5F),    Glow("lavatrl", kLava, 1.1F),
            Glow("lavatrl", kLavaCore, 0.5F),     Glow("lavaspsh", kLava, 1.1F),
            Glow("lavaspsh", kLavaCore, 0.5F),    Puff("lavasmk", kGreySmoke, 0.45F),
            Glow("ghsttrl", kGhost, 1.6F),        Glow("ghsttrl", kGhostCore, 0.6F),
            Glow("wizard", kMagic, 1.6F),         kMagicStars,
            Puff("watermst", kWhiteSmoke, 0.18F), kSnowflakes,
            Puff("smoke", kWhiteSmoke, 0.45F),    Puff("drgnsmok", kGreySmoke, 0.5F),
            Puff("flytrail", kWhiteSmoke, 0.3F),  kSplashDrops,
            Puff("splash", kWhiteSmoke, 0.25F)};
}

}
