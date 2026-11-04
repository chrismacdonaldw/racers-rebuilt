#include "app/race_fx.hpp"

#include <array>
#include <optional>

#include "core/log.hpp"
#include "render/fx_textures.hpp"

namespace rr::app {
namespace {

constexpr render::FxEffect kTireSmoke{.emitter = "tiresmk",
                                      .ratePerSecond = 55.0F,
                                      .burst = 2,
                                      .lifeMs = {.min = 1400.0F, .max = 2600.0F},
                                      .startSize = {.min = 1.2F, .max = 2.0F},
                                      .endSize = {.min = 7.0F, .max = 11.0F},
                                      .spinPerSecond = {.min = 0.2F, .max = 0.9F},
                                      .inherit = 0.1F,
                                      .spread = {.x = 3.0F, .y = 2.5F, .z = 3.0F},
                                      .acceleration = {.y = 2.5F},
                                      .drag = 1.5F,
                                      .color = {.r = 0.95F, .g = 0.95F, .b = 0.94F, .a = 0.5F},
                                      .shade = 0.45F,
                                      .fadeInShare = 0.08F,
                                      .fadeOutShare = 0.6F};

constexpr render::FxEffect kDriftDust{.emitter = "dust",
                                      .ratePerSecond = 35.0F,
                                      .burst = 4,
                                      .lifeMs = {.min = 900.0F, .max = 1600.0F},
                                      .startSize = {.min = 1.5F, .max = 2.5F},
                                      .endSize = {.min = 6.0F, .max = 9.0F},
                                      .spinPerSecond = {.min = 0.2F, .max = 1.0F},
                                      .inherit = 0.15F,
                                      .spread = {.x = 5.0F, .y = 2.0F, .z = 5.0F},
                                      .acceleration = {.y = 1.0F},
                                      .drag = 2.0F,
                                      .color = {.r = 0.80F, .g = 0.70F, .b = 0.55F, .a = 0.55F},
                                      .shade = 0.6F};

constexpr render::FxEffect kExhaustPuff{.emitter = "carsmke",
                                        .ratePerSecond = 30.0F,
                                        .burst = 5,
                                        .lifeMs = {.min = 700.0F, .max = 1100.0F},
                                        .startSize = {.min = 2.0F, .max = 3.0F},
                                        .endSize = {.min = 7.0F, .max = 10.0F},
                                        .spinPerSecond = {.min = 0.5F, .max = 1.5F},
                                        .inherit = 0.2F,
                                        .spread = {.x = 4.0F, .y = 3.0F, .z = 4.0F},
                                        .acceleration = {.y = 6.0F},
                                        .drag = 2.0F,
                                        .color = {.r = 0.5F, .g = 0.5F, .b = 0.52F, .a = 0.6F},
                                        .shade = 0.5F};

constexpr render::FxEffect kTurboSmoke{.emitter = "trbsmke",
                                       .ratePerSecond = 45.0F,
                                       .burst = 4,
                                       .lifeMs = {.min = 600.0F, .max = 1000.0F},
                                       .startSize = {.min = 3.0F, .max = 4.5F},
                                       .endSize = {.min = 8.0F, .max = 12.0F},
                                       .spinPerSecond = {.min = 0.5F, .max = 1.5F},
                                       .inherit = 0.3F,
                                       .spread = {.x = 5.0F, .y = 4.0F, .z = 5.0F},
                                       .acceleration = {.y = 5.0F},
                                       .drag = 2.0F,
                                       .color = {.r = 0.42F, .g = 0.42F, .b = 0.44F, .a = 0.55F},
                                       .shade = 0.4F};

constexpr render::FxEffect kLandingDust{.emitter = "carland",
                                        .burst = 18,
                                        .lifeMs = {.min = 900.0F, .max = 1500.0F},
                                        .startSize = {.min = 2.0F, .max = 3.0F},
                                        .endSize = {.min = 9.0F, .max = 13.0F},
                                        .spinPerSecond = {.min = 0.2F, .max = 0.8F},
                                        .inherit = 0.1F,
                                        .spread = {.x = 14.0F, .y = 3.0F, .z = 14.0F},
                                        .acceleration = {.y = 1.5F},
                                        .drag = 2.5F,
                                        .color = {.r = 0.82F, .g = 0.78F, .b = 0.70F, .a = 0.55F},
                                        .shade = 0.6F};

constexpr render::FxEffect kCannonSmoke{.emitter = "cannsmk",
                                        .ratePerSecond = 25.0F,
                                        .burst = 10,
                                        .lifeMs = {.min = 1100.0F, .max = 1800.0F},
                                        .startSize = {.min = 3.0F, .max = 5.0F},
                                        .endSize = {.min = 12.0F, .max = 18.0F},
                                        .spinPerSecond = {.min = 0.3F, .max = 1.0F},
                                        .inherit = 0.3F,
                                        .spread = {.x = 6.0F, .y = 5.0F, .z = 6.0F},
                                        .acceleration = {.y = 4.0F},
                                        .drag = 2.0F,
                                        .color = {.r = 0.7F, .g = 0.7F, .b = 0.7F, .a = 0.65F},
                                        .shade = 0.5F};

constexpr render::Color kSparkHot{.r = 1.0F, .g = 0.95F, .b = 0.7F};
constexpr render::Color kSparkCool{.r = 1.0F, .g = 0.35F, .b = 0.05F};

constexpr render::FxEffect Sparks(std::string_view emitter, int burst) {
    return {.emitter = emitter,
            .shape = render::FxShape::Streak,
            .burst = burst,
            .lifeMs = {.min = 350.0F, .max = 800.0F},
            .startSize = {.min = 0.8F, .max = 1.1F},
            .endSize = {.min = 0.4F, .max = 0.6F},
            .inherit = 0.4F,
            .launch = {.y = 12.0F},
            .spread = {.x = 30.0F, .y = 20.0F, .z = 30.0F},
            .acceleration = {.y = -60.0F},
            .drag = 0.8F,
            .streakSeconds = 0.06F,
            .bounce = 0.35F,
            .floorDrop = 1.0F,
            .color = kSparkHot,
            .endColor = kSparkCool,
            .fadeInShare = 0.0F,
            .fadeOutShare = 0.3F,
            .blend = render::BlendMode::Additive};
}

constexpr render::FxEffect Flash(std::string_view emitter, float size) {
    return {.emitter = emitter,
            .shape = render::FxShape::Glow,
            .burst = 1,
            .lifeMs = {.min = 90.0F, .max = 140.0F},
            .startSize = {.min = size, .max = size},
            .endSize = {.min = size * 0.4F, .max = size * 0.4F},
            .color = {.r = 1.0F, .g = 0.8F, .b = 0.5F, .a = 0.9F},
            .fadeInShare = 0.0F,
            .fadeOutShare = 1.0F,
            .blend = render::BlendMode::Additive};
}

constexpr render::FxEffect kCrashPuff{.emitter = "carsprk",
                                      .burst = 3,
                                      .lifeMs = {.min = 500.0F, .max = 800.0F},
                                      .startSize = {.min = 1.5F, .max = 2.0F},
                                      .endSize = {.min = 4.0F, .max = 6.0F},
                                      .spinPerSecond = {.min = 0.5F, .max = 1.5F},
                                      .inherit = 0.3F,
                                      .launch = {.y = 3.0F},
                                      .spread = {.x = 3.0F, .y = 2.0F, .z = 3.0F},
                                      .drag = 2.0F,
                                      .color = {.r = 0.5F, .g = 0.5F, .b = 0.5F, .a = 0.4F},
                                      .shade = 0.5F};

constexpr render::FxEffect kFuseSparks{.emitter = "dynsprk",
                                       .shape = render::FxShape::Streak,
                                       .ratePerSecond = 90.0F,
                                       .lifeMs = {.min = 200.0F, .max = 450.0F},
                                       .startSize = {.min = 0.5F, .max = 0.7F},
                                       .endSize = {.min = 0.25F, .max = 0.35F},
                                       .launch = {.y = 6.0F},
                                       .spread = {.x = 10.0F, .y = 8.0F, .z = 10.0F},
                                       .acceleration = {.y = -40.0F},
                                       .streakSeconds = 0.05F,
                                       .color = kSparkHot,
                                       .endColor = kSparkCool,
                                       .fadeInShare = 0.0F,
                                       .fadeOutShare = 0.3F,
                                       .blend = render::BlendMode::Additive};

constexpr render::FxEffect kFuseGlow{.emitter = "dynsprk",
                                     .shape = render::FxShape::Glow,
                                     .ratePerSecond = 15.0F,
                                     .lifeMs = {.min = 60.0F, .max = 120.0F},
                                     .startSize = {.min = 3.0F, .max = 4.0F},
                                     .endSize = {.min = 2.0F, .max = 2.5F},
                                     .color = {.r = 1.0F, .g = 0.75F, .b = 0.4F, .a = 0.8F},
                                     .fadeInShare = 0.0F,
                                     .fadeOutShare = 1.0F,
                                     .blend = render::BlendMode::Additive};

constexpr std::array kBrickPaints{render::Color{.r = 0.78F, .g = 0.08F, .b = 0.06F},
                                  render::Color{.r = 0.0F, .g = 0.33F, .b = 0.75F},
                                  render::Color{.r = 0.98F, .g = 0.78F, .b = 0.0F},
                                  render::Color{.r = 0.0F, .g = 0.52F, .b = 0.2F},
                                  render::Color{.r = 0.95F, .g = 0.95F, .b = 0.95F},
                                  render::Color{.r = 0.12F, .g = 0.12F, .b = 0.13F}};

constexpr render::FxEffect kBlastFlash{.emitter = kBlastEffect,
                                       .shape = render::FxShape::Glow,
                                       .burst = 1,
                                       .lifeMs = {.min = 160.0F, .max = 220.0F},
                                       .startSize = {.min = 28.0F, .max = 28.0F},
                                       .endSize = {.min = 12.0F, .max = 12.0F},
                                       .color = {.r = 1.0F, .g = 0.85F, .b = 0.6F},
                                       .fadeInShare = 0.0F,
                                       .fadeOutShare = 1.0F,
                                       .blend = render::BlendMode::Additive};

constexpr render::FxEffect kFireball{
    .emitter = kBlastEffect,
    .burst = 16,
    .lifeMs = {.min = 450.0F, .max = 800.0F},
    .startSize = {.min = 4.0F, .max = 6.0F},
    .endSize = {.min = 12.0F, .max = 18.0F},
    .spinPerSecond = {.min = 0.5F, .max = 1.5F},
    .launch = {.y = 8.0F},
    .spread = {.x = 16.0F, .y = 10.0F, .z = 16.0F},
    .drag = 3.0F,
    .color = {.r = 1.0F, .g = 0.65F, .b = 0.25F, .a = 0.95F},
    .endColor = render::Color{.r = 0.18F, .g = 0.16F, .b = 0.15F, .a = 0.55F},
    .shade = 0.75F,
    .fadeInShare = 0.02F};

constexpr render::FxEffect kBlastFire{.emitter = kBlastEffect,
                                      .shape = render::FxShape::Glow,
                                      .burst = 8,
                                      .lifeMs = {.min = 300.0F, .max = 550.0F},
                                      .startSize = {.min = 8.0F, .max = 11.0F},
                                      .endSize = {.min = 14.0F, .max = 18.0F},
                                      .launch = {.y = 6.0F},
                                      .spread = {.x = 12.0F, .y = 8.0F, .z = 12.0F},
                                      .drag = 3.0F,
                                      .color = {.r = 1.0F, .g = 0.6F, .b = 0.15F},
                                      .endColor = render::Color{.r = 0.8F, .g = 0.15F, .b = 0.02F},
                                      .fadeInShare = 0.0F,
                                      .fadeOutShare = 0.7F,
                                      .blend = render::BlendMode::Additive};

constexpr render::FxEffect kBlastSmoke{.emitter = kBlastEffect,
                                       .burst = 10,
                                       .lifeMs = {.min = 1800.0F, .max = 3000.0F},
                                       .startSize = {.min = 6.0F, .max = 8.0F},
                                       .endSize = {.min = 18.0F, .max = 24.0F},
                                       .spinPerSecond = {.min = 0.2F, .max = 0.6F},
                                       .launch = {.y = 7.0F},
                                       .spread = {.x = 8.0F, .y = 4.0F, .z = 8.0F},
                                       .acceleration = {.y = 2.0F},
                                       .drag = 1.5F,
                                       .color = {.r = 0.4F, .g = 0.38F, .b = 0.36F, .a = 0.5F},
                                       .shade = 0.5F,
                                       .fadeInShare = 0.15F,
                                       .fadeOutShare = 0.6F};

constexpr render::FxEffect kEmbers{.emitter = kBlastEffect,
                                   .shape = render::FxShape::Streak,
                                   .burst = 26,
                                   .lifeMs = {.min = 600.0F, .max = 1300.0F},
                                   .startSize = {.min = 0.7F, .max = 0.9F},
                                   .endSize = {.min = 0.3F, .max = 0.4F},
                                   .launch = {.y = 22.0F},
                                   .spread = {.x = 30.0F, .y = 18.0F, .z = 30.0F},
                                   .acceleration = {.y = -50.0F},
                                   .drag = 0.4F,
                                   .streakSeconds = 0.03F,
                                   .bounce = 0.3F,
                                   .floorDrop = 5.0F,
                                   .color = kSparkHot,
                                   .endColor = kSparkCool,
                                   .fadeInShare = 0.0F,
                                   .fadeOutShare = 0.3F,
                                   .blend = render::BlendMode::Additive};

constexpr render::FxEffect kShockwave{.emitter = kBlastEffect,
                                      .shape = render::FxShape::Ring,
                                      .burst = 1,
                                      .lifeMs = {.min = 450.0F, .max = 450.0F},
                                      .startSize = {.min = 4.0F, .max = 4.0F},
                                      .endSize = {.min = 46.0F, .max = 46.0F},
                                      .color = {.r = 1.0F, .g = 0.8F, .b = 0.55F, .a = 0.7F},
                                      .offset = {.y = -4.5F},
                                      .fadeInShare = 0.0F,
                                      .fadeOutShare = 0.8F,
                                      .blend = render::BlendMode::Additive};

constexpr render::FxEffect kFlyingBricks{.emitter = kBlastEffect,
                                         .shape = render::FxShape::Brick,
                                         .burst = 9,
                                         .lifeMs = {.min = 1800.0F, .max = 2400.0F},
                                         .startSize = {.min = 2.2F, .max = 3.0F},
                                         .spinPerSecond = {.min = 4.0F, .max = 10.0F},
                                         .launch = {.y = 26.0F},
                                         .spread = {.x = 20.0F, .y = 12.0F, .z = 20.0F},
                                         .acceleration = {.y = -60.0F},
                                         .drag = 0.2F,
                                         .bounce = 0.45F,
                                         .floorDrop = 5.0F,
                                         .palette = kBrickPaints,
                                         .fadeInShare = 0.0F,
                                         .fadeOutShare = 0.15F};

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
    return {kTireSmoke,
            kDriftDust,
            kExhaustPuff,
            kTurboSmoke,
            kLandingDust,
            kCannonSmoke,
            Sparks("carsprk", 36),
            Flash("carsprk", 10.0F),
            kCrashPuff,
            Sparks(kWallSparksEffect, 12),
            Flash(kWallSparksEffect, 6.0F),
            kFuseSparks,
            kFuseGlow,
            kBlastFlash,
            kFireball,
            kBlastFire,
            kBlastSmoke,
            kEmbers,
            kShockwave,
            kFlyingBricks};
}

std::unique_ptr<render::FxSystem> MakeRaceFx(render::Renderer& renderer) {
    const std::optional<render::TextureId> smoke = Upload(renderer, render::MakeSmokeAtlas());
    const std::optional<render::TextureId> spark =
        smoke ? Upload(renderer, render::MakeSparkTexture()) : std::nullopt;
    const std::optional<render::TextureId> glow =
        spark ? Upload(renderer, render::MakeGlowTexture()) : std::nullopt;
    const std::optional<render::TextureId> ring =
        glow ? Upload(renderer, render::MakeRingTexture()) : std::nullopt;
    if (!ring) {
        return nullptr;
    }
    Result<render::MeshId> brick = renderer.UploadMesh(render::MakeBrickGeometry());
    if (!brick) {
        log::Warn("{}, racing without better particles", brick.error().message);
        return nullptr;
    }
    return std::make_unique<render::FxSystem>(
        RaceFxEffects(),
        render::FxAssets{
            .smoke = *smoke, .spark = *spark, .glow = *glow, .ring = *ring, .brick = *brick});
}

}
