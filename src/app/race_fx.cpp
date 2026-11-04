#include "app/race_fx.hpp"

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

}

std::vector<render::FxEffect> RaceFxEffects() {
    return {kTireSmoke, kDriftDust, kExhaustPuff, kTurboSmoke, kLandingDust, kCannonSmoke};
}

}
