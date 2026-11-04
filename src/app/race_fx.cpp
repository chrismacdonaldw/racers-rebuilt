#include "app/race_fx.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <iterator>
#include <optional>

#include "app/hazard/hazard_fx_effects.hpp"
#include "app/powerup/powerup_fx_effects.hpp"
#include "app/racer/surface_fx_effects.hpp"
#include "core/log.hpp"
#include "render/fx_textures.hpp"

namespace rr::app {
namespace {

using render::BlendMode;
using render::Color;
using render::FxEffect;
using render::FxShape;

constexpr Color kSmoke{.r = 0.96F, .g = 0.96F, .b = 0.95F};
constexpr Color kDust{.r = 0.72F, .g = 0.7F, .b = 0.66F};
constexpr Color kCarSpark{.r = 0.7F, .g = 0.65F, .b = 1.0F};
constexpr Color kFuseSpark{.r = 1.0F, .g = 0.62F, .b = 0.18F};
constexpr Color kBlastHot{.r = 1.0F, .g = 0.72F, .b = 0.18F};
constexpr Color kBlastCool{.r = 0.95F, .g = 0.22F, .b = 0.03F, .a = 0.0F};

constexpr FxEffect Puff(std::string_view emitter, float scale, float growth, float alpha) {
    return {.emitter = emitter,
            .lifeMs = {.min = 0.85F, .max = 1.15F},
            .startSize = {.min = 0.85F * scale, .max = 1.1F * scale},
            .endSize = {.min = 0.9F * scale * growth, .max = 1.2F * scale * growth},
            .spinPerSecond = {.min = 0.2F, .max = 0.8F},
            .inherit = 1.0F,
            .spread = {.x = 1.0F, .y = 1.0F, .z = 1.0F},
            .drag = 0.5F,
            .color = {.r = kSmoke.r, .g = kSmoke.g, .b = kSmoke.b, .a = alpha},
            .shade = 0.72F,
            .fadeInShare = 0.12F,
            .fadeOutShare = 0.6F,
            .followsEmitter = true};
}

constexpr FxEffect Wisp(std::string_view emitter, float scale, float growth, float alpha) {
    return {.emitter = emitter,
            .lifeMs = {.min = 0.7F, .max = 0.9F},
            .startSize = {.min = 0.5F * scale, .max = 0.7F * scale},
            .endSize = {.min = 0.6F * scale * growth, .max = 0.8F * scale * growth},
            .spinPerSecond = {.min = 0.4F, .max = 1.2F},
            .inherit = 0.9F,
            .spread = {.x = 3.0F, .y = 2.0F, .z = 3.0F},
            .drag = 0.8F,
            .color = {.r = kSmoke.r, .g = kSmoke.g, .b = kSmoke.b, .a = alpha},
            .shade = 0.8F,
            .fadeInShare = 0.15F,
            .fadeOutShare = 0.6F,
            .followsEmitter = true};
}

constexpr FxEffect Twinkle(std::string_view emitter, Color color, float scale) {
    return {.emitter = emitter,
            .shape = FxShape::Star,
            .lifeMs = {.min = 0.9F, .max = 1.1F},
            .startSize = {.min = scale, .max = scale * 1.2F},
            .endSize = {.min = scale * 0.8F, .max = scale},
            .spinPerSecond = {.min = 2.0F, .max = 6.0F},
            .inherit = 1.0F,
            .color = color,
            .fadeInShare = 0.0F,
            .fadeOutShare = 0.35F,
            .blend = BlendMode::Additive,
            .followsEmitter = true};
}

constexpr FxEffect kDustGlints{.emitter = "dust",
                               .shape = FxShape::Star,
                               .lifeMs = {.min = 0.9F, .max = 1.1F},
                               .startSize = {.min = 0.9F, .max = 1.1F},
                               .endSize = {.min = 0.9F, .max = 1.1F},
                               .spinPerSecond = {.min = 1.0F, .max = 3.0F},
                               .inherit = 1.0F,
                               .color = {.r = kDust.r, .g = kDust.g, .b = kDust.b, .a = 0.85F},
                               .fadeInShare = 0.05F,
                               .fadeOutShare = 0.5F,
                               .followsEmitter = true};

constexpr FxEffect kCarSparkPop{.emitter = "carsprk",
                                .shape = FxShape::Star,
                                .burst = 1,
                                .lifeMs = {.min = 160.0F, .max = 160.0F},
                                .startSize = {.min = 5.0F, .max = 5.0F},
                                .endSize = {.min = 1.5F, .max = 1.5F},
                                .color = kCarSpark,
                                .fadeInShare = 0.0F,
                                .fadeOutShare = 1.0F,
                                .blend = BlendMode::Additive};

constexpr FxEffect kFuseGlow{.emitter = "dynsprk",
                             .shape = FxShape::Glow,
                             .ratePerSecond = 15.0F,
                             .lifeMs = {.min = 60.0F, .max = 120.0F},
                             .startSize = {.min = 3.0F, .max = 4.0F},
                             .endSize = {.min = 2.0F, .max = 2.5F},
                             .color = {.r = 1.0F, .g = 0.75F, .b = 0.4F, .a = 0.8F},
                             .fadeInShare = 0.0F,
                             .fadeOutShare = 1.0F,
                             .blend = BlendMode::Additive};

constexpr FxEffect kFuseWisps{.emitter = "dynsprk",
                              .ratePerSecond = 8.0F,
                              .lifeMs = {.min = 600.0F, .max = 900.0F},
                              .startSize = {.min = 0.6F, .max = 0.9F},
                              .endSize = {.min = 2.0F, .max = 2.8F},
                              .spinPerSecond = {.min = 0.5F, .max = 1.5F},
                              .launch = {.y = 4.0F},
                              .spread = {.x = 1.0F, .y = 1.0F, .z = 1.0F},
                              .color = {.r = 0.85F, .g = 0.85F, .b = 0.85F, .a = 0.3F},
                              .shade = 0.7F};

constexpr FxEffect kBlastFlash{.emitter = kBlastEffect,
                               .shape = FxShape::Glow,
                               .burst = 1,
                               .lifeMs = {.min = 120.0F, .max = 120.0F},
                               .startSize = {.min = 24.0F, .max = 24.0F},
                               .endSize = {.min = 14.0F, .max = 14.0F},
                               .fadeInShare = 0.0F,
                               .fadeOutShare = 1.0F,
                               .blend = BlendMode::Additive};

constexpr FxEffect kBlastCore{.emitter = kBlastEffect,
                              .shape = FxShape::Glow,
                              .burst = 1,
                              .lifeMs = {.min = 350.0F, .max = 350.0F},
                              .startSize = {.min = 34.0F, .max = 34.0F},
                              .endSize = {.min = 20.0F, .max = 20.0F},
                              .color = kBlastHot,
                              .endColor = kBlastCool,
                              .fadeInShare = 0.0F,
                              .fadeOutShare = 0.3F,
                              .blend = BlendMode::Additive};

constexpr FxEffect kBlastPuffs{.emitter = kBlastEffect,
                               .burst = 6,
                               .lifeMs = {.min = 900.0F, .max = 1300.0F},
                               .startSize = {.min = 6.0F, .max = 8.0F},
                               .endSize = {.min = 12.0F, .max = 16.0F},
                               .spinPerSecond = {.min = 0.2F, .max = 0.6F},
                               .launch = {.y = 6.0F},
                               .spread = {.x = 10.0F, .y = 4.0F, .z = 10.0F},
                               .drag = 2.0F,
                               .color = {.r = kSmoke.r, .g = kSmoke.g, .b = kSmoke.b, .a = 0.6F},
                               .shade = 0.72F,
                               .fadeInShare = 0.3F,
                               .fadeOutShare = 0.6F};

std::optional<render::TextureId> Upload(render::Renderer& renderer,
                                        const assets::ImageAsset& image) {
    Result<render::TextureId> texture = renderer.UploadTexture(image);
    if (!texture) {
        log::Warn("{}, racing without better particles", texture.error().message);
        return std::nullopt;
    }
    return *texture;
}

}

std::vector<render::FxEffect> RaceFxEffects() {
    std::vector<render::FxEffect> effects{Puff("tiresmk", 1.6F, 2.0F, 0.42F),
                                          Wisp("tiresmk", 1.6F, 2.0F, 0.26F),
                                          kDustGlints,
                                          Puff("dust", 1.4F, 1.0F, 0.2F),
                                          Puff("carsmke", 1.0F, 1.0F, 0.38F),
                                          Wisp("carsmke", 1.0F, 1.0F, 0.22F),
                                          Puff("trbsmke", 1.0F, 1.0F, 0.45F),
                                          Wisp("trbsmke", 1.0F, 1.0F, 0.26F),
                                          Puff("carland", 1.0F, 1.0F, 0.4F),
                                          Wisp("carland", 1.0F, 1.0F, 0.22F),
                                          Puff("cannsmk", 1.0F, 1.0F, 0.5F),
                                          Wisp("cannsmk", 1.0F, 1.0F, 0.26F),
                                          Twinkle("carsprk", kCarSpark, 1.6F),
                                          kCarSparkPop,
                                          Twinkle("dynsprk", kFuseSpark, 1.6F),
                                          kFuseGlow,
                                          kFuseWisps,
                                          kBlastFlash,
                                          kBlastCore,
                                          kBlastPuffs};
    std::ranges::copy(PowerupFxEffects(), std::back_inserter(effects));
    std::ranges::copy(HazardFxEffects(), std::back_inserter(effects));
    std::ranges::copy(SurfaceFxEffects(), std::back_inserter(effects));
    return effects;
}

std::unique_ptr<render::FxSystem> MakeRaceFx(render::Renderer& renderer) {
    constexpr std::array kMakers{render::MakeSmokeAtlas,    render::MakeSparkTexture,
                                 render::MakeGlowTexture,   render::MakeRingTexture,
                                 render::MakeBubbleTexture, render::MakeStarTexture};
    std::array<render::TextureId, kMakers.size()> textures{};
    for (std::size_t i = 0; i < kMakers.size(); ++i) {
        const std::optional<render::TextureId> texture = Upload(renderer, kMakers.at(i)());
        if (!texture) {
            return nullptr;
        }
        textures.at(i) = *texture;
    }
    const auto [smoke, spark, glow, ring, bubble, star] = textures;
    return std::make_unique<render::FxSystem>(RaceFxEffects(), render::FxAssets{.smoke = smoke,
                                                                                .spark = spark,
                                                                                .glow = glow,
                                                                                .ring = ring,
                                                                                .bubble = bubble,
                                                                                .star = star});
}

}
