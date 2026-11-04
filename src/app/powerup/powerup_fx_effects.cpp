#include "app/powerup/powerup_fx_effects.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <span>

namespace rr::app {
namespace {

using render::BlendMode;
using render::Color;
using render::FxEffect;
using render::FxShape;

constexpr Color kFlameHot{.r = 1.0F, .g = 0.85F, .b = 0.45F, .a = 0.85F};
constexpr Color kFlameCool{.r = 0.9F, .g = 0.2F, .b = 0.04F, .a = 0.0F};
constexpr Color kTurboCore{.r = 0.75F, .g = 0.85F, .b = 1.0F, .a = 0.9F};
constexpr Color kHoverCore{.r = 0.35F, .g = 0.95F, .b = 0.6F, .a = 0.9F};
constexpr Color kEmberHot{.r = 1.0F, .g = 0.95F, .b = 0.7F};
constexpr Color kEmberCool{.r = 1.0F, .g = 0.35F, .b = 0.05F};
constexpr Color kBolt{.r = 0.85F, .g = 0.92F, .b = 1.0F};
constexpr Color kBoltFade{.r = 0.35F, .g = 0.45F, .b = 1.0F};
constexpr Color kWarp{.r = 0.5F, .g = 0.9F, .b = 1.0F};
constexpr Color kWarpFade{.r = 0.25F, .g = 0.35F, .b = 1.0F};
constexpr Color kMagnet{.r = 0.6F, .g = 0.7F, .b = 1.0F};
constexpr Color kMagnetFade{.r = 0.8F, .g = 0.4F, .b = 1.0F};

constexpr std::array<Color, sim::kPowerupColourCount> kPickupGlows{
    Color{.r = 1.0F, .g = 0.3F, .b = 0.25F}, Color{.r = 0.35F, .g = 0.6F, .b = 1.0F},
    Color{.r = 0.35F, .g = 1.0F, .b = 0.4F}, Color{.r = 1.0F, .g = 0.88F, .b = 0.3F}};
constexpr std::array<std::array<Color, 1>, sim::kPowerupColourCount> kPickupPaints{
    {{Color{.r = 0.78F, .g = 0.08F, .b = 0.06F}},
     {Color{.r = 0.0F, .g = 0.33F, .b = 0.75F}},
     {Color{.r = 0.0F, .g = 0.52F, .b = 0.2F}},
     {Color{.r = 0.98F, .g = 0.78F, .b = 0.0F}}}};
constexpr Color kWhiteGlow{};
constexpr std::array<Color, 1> kWhitePaint{Color{.r = 0.95F, .g = 0.95F, .b = 0.95F}};

constexpr FxEffect Flash(std::string_view emitter, float size, Color color, float lifeMs) {
    return {.emitter = emitter,
            .shape = FxShape::Glow,
            .burst = 1,
            .lifeMs = {.min = lifeMs, .max = lifeMs},
            .startSize = {.min = size, .max = size},
            .endSize = {.min = size * 0.4F, .max = size * 0.4F},
            .color = color,
            .fadeInShare = 0.0F,
            .fadeOutShare = 1.0F,
            .blend = BlendMode::Additive};
}

constexpr FxEffect Ring(std::string_view emitter, float from, float to, Color color) {
    return {.emitter = emitter,
            .shape = FxShape::Ring,
            .burst = 1,
            .lifeMs = {.min = 400.0F, .max = 400.0F},
            .startSize = {.min = from, .max = from},
            .endSize = {.min = to, .max = to},
            .color = {.r = color.r, .g = color.g, .b = color.b, .a = 0.6F},
            .fadeInShare = 0.0F,
            .fadeOutShare = 0.8F,
            .blend = BlendMode::Additive};
}

constexpr FxEffect FlameCore(std::string_view emitter, float scale, Color color) {
    return {.emitter = emitter,
            .shape = FxShape::Glow,
            .ratePerSecond = 90.0F,
            .lifeMs = {.min = 90.0F, .max = 150.0F},
            .startSize = {.min = 2.8F * scale, .max = 3.4F * scale},
            .endSize = {.min = 0.8F * scale, .max = 1.2F * scale},
            .inherit = 1.0F,
            .spread = {.x = 2.0F, .y = 2.0F, .z = 2.0F},
            .color = color,
            .endColor = Color{.r = 1.0F, .g = 0.6F, .b = 0.2F, .a = 0.6F},
            .fadeInShare = 0.0F,
            .fadeOutShare = 0.6F,
            .blend = BlendMode::Additive};
}

constexpr FxEffect Flame(std::string_view emitter, float scale) {
    return {.emitter = emitter,
            .shape = FxShape::Glow,
            .ratePerSecond = 120.0F,
            .lifeMs = {.min = 200.0F, .max = 350.0F},
            .startSize = {.min = 2.2F * scale, .max = 3.0F * scale},
            .endSize = {.min = 5.0F * scale, .max = 7.0F * scale},
            .inherit = 1.0F,
            .spread = {.x = 6.0F, .y = 4.0F, .z = 6.0F},
            .acceleration = {.y = 10.0F},
            .drag = 2.0F,
            .color = kFlameHot,
            .endColor = kFlameCool,
            .fadeInShare = 0.05F,
            .fadeOutShare = 0.5F,
            .blend = BlendMode::Additive};
}

constexpr FxEffect Embers(std::string_view emitter, float rate) {
    return {.emitter = emitter,
            .shape = FxShape::Streak,
            .ratePerSecond = rate,
            .lifeMs = {.min = 200.0F, .max = 400.0F},
            .startSize = {.min = 0.3F, .max = 0.4F},
            .endSize = {.min = 0.15F, .max = 0.2F},
            .inherit = 0.8F,
            .spread = {.x = 15.0F, .y = 10.0F, .z = 15.0F},
            .acceleration = {.y = -40.0F},
            .streakSeconds = 0.04F,
            .color = kEmberHot,
            .endColor = kEmberCool,
            .fadeInShare = 0.0F,
            .fadeOutShare = 0.3F,
            .blend = BlendMode::Additive};
}

constexpr FxEffect Glints(std::string_view emitter, int burst, Color color) {
    return {.emitter = emitter,
            .shape = FxShape::Star,
            .burst = burst,
            .lifeMs = {.min = 500.0F, .max = 900.0F},
            .startSize = {.min = 1.6F, .max = 2.2F},
            .endSize = {.min = 0.2F, .max = 0.4F},
            .spinPerSecond = {.min = 2.0F, .max = 5.0F},
            .launch = {.y = 10.0F},
            .spread = {.x = 14.0F, .y = 10.0F, .z = 14.0F},
            .acceleration = {.y = -8.0F},
            .drag = 1.5F,
            .color = color,
            .fadeInShare = 0.0F,
            .fadeOutShare = 0.5F,
            .blend = BlendMode::Additive};
}

constexpr FxEffect MiniBricks(std::string_view emitter, std::span<const Color> paint) {
    return {.emitter = emitter,
            .shape = FxShape::Brick,
            .burst = 4,
            .lifeMs = {.min = 700.0F, .max = 1000.0F},
            .startSize = {.min = 0.7F, .max = 0.9F},
            .spinPerSecond = {.min = 6.0F, .max = 12.0F},
            .launch = {.y = 14.0F},
            .spread = {.x = 10.0F, .y = 6.0F, .z = 10.0F},
            .acceleration = {.y = -45.0F},
            .palette = paint,
            .fadeInShare = 0.0F,
            .fadeOutShare = 0.3F};
}

constexpr std::array<FxEffect, 4> Pickup(std::string_view emitter, Color glow,
                                         std::span<const Color> paint) {
    return {Flash(emitter, 9.0F, glow, 250.0F), Ring(emitter, 2.0F, 14.0F, glow),
            Glints(emitter, 14, glow), MiniBricks(emitter, paint)};
}

constexpr FxEffect kStrikeCrackle{.emitter = "lghthit",
                                  .shape = FxShape::Streak,
                                  .ratePerSecond = 60.0F,
                                  .burst = 20,
                                  .lifeMs = {.min = 150.0F, .max = 350.0F},
                                  .startSize = {.min = 0.35F, .max = 0.5F},
                                  .endSize = {.min = 0.2F, .max = 0.3F},
                                  .launch = {.y = 8.0F},
                                  .spread = {.x = 40.0F, .y = 30.0F, .z = 40.0F},
                                  .drag = 1.0F,
                                  .streakSeconds = 0.03F,
                                  .color = kBolt,
                                  .endColor = kBoltFade,
                                  .fadeInShare = 0.0F,
                                  .blend = BlendMode::Additive};

constexpr FxEffect kStrikeGlow{.emitter = "lghthit",
                               .shape = FxShape::Glow,
                               .ratePerSecond = 20.0F,
                               .lifeMs = {.min = 120.0F, .max = 200.0F},
                               .startSize = {.min = 5.0F, .max = 7.0F},
                               .endSize = {.min = 3.0F, .max = 3.0F},
                               .color = {.r = 0.5F, .g = 0.65F, .b = 1.0F, .a = 0.6F},
                               .fadeInShare = 0.0F,
                               .fadeOutShare = 0.8F,
                               .blend = BlendMode::Additive};

constexpr FxEffect kOilBubbles{.emitter = "oilbub",
                               .shape = FxShape::Bubble,
                               .ratePerSecond = 14.0F,
                               .lifeMs = {.min = 700.0F, .max = 1400.0F},
                               .startSize = {.min = 1.2F, .max = 1.8F},
                               .endSize = {.min = 2.6F, .max = 3.6F},
                               .launch = {.y = 2.0F},
                               .spread = {.x = 0.5F, .y = 0.5F, .z = 0.5F},
                               .color = {.a = 0.9F},
                               .fadeInShare = 0.3F,
                               .fadeOutShare = 0.12F};

constexpr FxEffect kWarpSwirl{.emitter = kWarpSwirlEffect,
                              .shape = FxShape::Star,
                              .ratePerSecond = 40.0F,
                              .lifeMs = {.min = 500.0F, .max = 700.0F},
                              .startSize = {.min = 1.0F, .max = 1.6F},
                              .endSize = {.min = 0.2F, .max = 0.3F},
                              .spinPerSecond = {.min = 2.0F, .max = 4.0F},
                              .launch = {.y = 5.0F},
                              .color = kWarp,
                              .endColor = kWarpFade,
                              .spawnRadius = 7.0F,
                              .radialSpeed = -12.0F,
                              .fadeInShare = 0.2F,
                              .fadeOutShare = 0.3F,
                              .blend = BlendMode::Additive};

constexpr FxEffect kWarpStreaks{.emitter = kWarpSwirlEffect,
                                .shape = FxShape::Streak,
                                .ratePerSecond = 30.0F,
                                .lifeMs = {.min = 300.0F, .max = 450.0F},
                                .startSize = {.min = 0.3F, .max = 0.4F},
                                .endSize = {.min = 0.2F, .max = 0.2F},
                                .launch = {.y = 3.0F},
                                .streakSeconds = 0.05F,
                                .color = kWarp,
                                .endColor = kWarpFade,
                                .spawnRadius = 9.0F,
                                .radialSpeed = -20.0F,
                                .fadeInShare = 0.2F,
                                .fadeOutShare = 0.3F,
                                .blend = BlendMode::Additive};

constexpr FxEffect WarpStars(std::string_view emitter, int burst, float speed) {
    return {.emitter = emitter,
            .shape = FxShape::Star,
            .burst = burst,
            .lifeMs = {.min = 500.0F, .max = 900.0F},
            .startSize = {.min = 1.4F, .max = 2.0F},
            .endSize = {.min = 0.2F, .max = 0.3F},
            .spinPerSecond = {.min = 2.0F, .max = 5.0F},
            .launch = {.y = 8.0F},
            .spread = {.x = 4.0F, .y = 8.0F, .z = 4.0F},
            .drag = 1.5F,
            .color = kWarp,
            .endColor = kWarpFade,
            .spawnRadius = 1.0F,
            .radialSpeed = speed,
            .fadeInShare = 0.0F,
            .fadeOutShare = 0.5F,
            .blend = BlendMode::Additive};
}

constexpr FxEffect kWarpInSparks{.emitter = kWarpInEffect,
                                 .shape = FxShape::Streak,
                                 .burst = 12,
                                 .lifeMs = {.min = 300.0F, .max = 500.0F},
                                 .startSize = {.min = 0.35F, .max = 0.5F},
                                 .endSize = {.min = 0.2F, .max = 0.3F},
                                 .launch = {.y = 25.0F},
                                 .spread = {.x = 6.0F, .y = 6.0F, .z = 6.0F},
                                 .acceleration = {.y = -30.0F},
                                 .streakSeconds = 0.04F,
                                 .color = kWarp,
                                 .endColor = kWarpFade,
                                 .fadeInShare = 0.0F,
                                 .blend = BlendMode::Additive};

constexpr FxEffect kMagnetPull{.emitter = kMagnetPullEffect,
                               .shape = FxShape::Streak,
                               .ratePerSecond = 60.0F,
                               .lifeMs = {.min = 250.0F, .max = 450.0F},
                               .startSize = {.min = 0.5F, .max = 0.7F},
                               .endSize = {.min = 0.3F, .max = 0.4F},
                               .inherit = 1.0F,
                               .spread = {.x = 6.0F, .y = 6.0F, .z = 6.0F},
                               .streakSeconds = 0.08F,
                               .color = kMagnet,
                               .endColor = kMagnetFade,
                               .spawnRadius = 3.0F,
                               .fadeInShare = 0.1F,
                               .fadeOutShare = 0.4F,
                               .blend = BlendMode::Additive};

constexpr FxEffect kMagnetGlints{.emitter = kMagnetPullEffect,
                                 .shape = FxShape::Star,
                                 .ratePerSecond = 25.0F,
                                 .lifeMs = {.min = 400.0F, .max = 600.0F},
                                 .startSize = {.min = 1.2F, .max = 1.8F},
                                 .endSize = {.min = 0.2F, .max = 0.3F},
                                 .spinPerSecond = {.min = 2.0F, .max = 4.0F},
                                 .inherit = 0.6F,
                                 .spread = {.x = 4.0F, .y = 4.0F, .z = 4.0F},
                                 .color = kMagnet,
                                 .endColor = kMagnetFade,
                                 .spawnRadius = 3.0F,
                                 .fadeInShare = 0.1F,
                                 .fadeOutShare = 0.4F,
                                 .blend = BlendMode::Additive};

constexpr FxEffect kMagnetPulse{.emitter = kMagnetPulseEffect,
                                .shape = FxShape::Ring,
                                .ratePerSecond = 3.0F,
                                .burst = 1,
                                .lifeMs = {.min = 700.0F, .max = 700.0F},
                                .startSize = {.min = 18.0F, .max = 18.0F},
                                .endSize = {.min = 3.0F, .max = 3.0F},
                                .color = {.r = 0.55F, .g = 0.65F, .b = 1.0F, .a = 0.8F},
                                .offset = {.y = -1.5F},
                                .fadeInShare = 0.3F,
                                .fadeOutShare = 0.3F,
                                .blend = BlendMode::Additive};

template <std::size_t Count>
void Append(std::vector<FxEffect>& effects, const std::array<FxEffect, Count>& more) {
    std::ranges::copy(more, std::back_inserter(effects));
}

}

std::vector<render::FxEffect> PowerupFxEffects() {
    std::vector<FxEffect> effects{FlameCore(kTurboFlameEffect, 1.0F, kTurboCore),
                                  Flame(kTurboFlameEffect, 1.0F),
                                  Embers(kTurboFlameEffect, 25.0F),
                                  FlameCore(kHoverFlameEffect, 1.4F, kHoverCore),
                                  Flame(kHoverFlameEffect, 1.4F),
                                  Embers(kHoverFlameEffect, 40.0F),
                                  Flash("lghthit", 16.0F, kBolt, 160.0F),
                                  kStrikeCrackle,
                                  kStrikeGlow,
                                  Ring("lghthit", 3.0F, 20.0F, kBolt),
                                  kOilBubbles,
                                  kWarpSwirl,
                                  kWarpStreaks,
                                  Flash(kWarpOutEffect, 14.0F, kWarp, 220.0F),
                                  WarpStars(kWarpOutEffect, 24, 18.0F),
                                  Ring(kWarpOutEffect, 3.0F, 26.0F, kWarp),
                                  Flash(kWarpInEffect, 16.0F, kWarp, 220.0F),
                                  WarpStars(kWarpInEffect, 30, 22.0F),
                                  Ring(kWarpInEffect, 3.0F, 26.0F, kWarp),
                                  kWarpInSparks,
                                  kMagnetPull,
                                  kMagnetGlints,
                                  kMagnetPulse};
    for (std::size_t colour = 0; colour < kPickupEffects.size(); ++colour) {
        Append(effects, Pickup(kPickupEffects.at(colour), kPickupGlows.at(colour),
                               kPickupPaints.at(colour)));
    }
    Append(effects, Pickup(kWhitePickupEffect, kWhiteGlow, kWhitePaint));
    return effects;
}

}
