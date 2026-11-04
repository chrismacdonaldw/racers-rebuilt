#include "app/racer/surface_fx_effects.hpp"

#include <string_view>

namespace rr::app {
namespace {

using render::BlendMode;
using render::Color;
using render::FxEffect;
using render::FxShape;

constexpr FxEffect Flecks(std::string_view emitter, float rate, Color color, Color endColor) {
    return {.emitter = emitter,
            .shape = FxShape::Streak,
            .ratePerSecond = rate,
            .lifeMs = {.min = 300.0F, .max = 600.0F},
            .startSize = {.min = 0.4F, .max = 0.6F},
            .endSize = {.min = 0.3F, .max = 0.45F},
            .inherit = 0.4F,
            .launch = {.y = 10.0F},
            .spread = {.x = 8.0F, .y = 5.0F, .z = 8.0F},
            .acceleration = {.y = -40.0F},
            .drag = 0.5F,
            .streakSeconds = 0.05F,
            .color = color,
            .endColor = endColor,
            .fadeInShare = 0.0F,
            .fadeOutShare = 0.3F};
}

constexpr FxEffect Droplets(std::string_view emitter, float rate, Color color) {
    return {.emitter = emitter,
            .shape = FxShape::Streak,
            .ratePerSecond = rate,
            .lifeMs = {.min = 400.0F, .max = 700.0F},
            .startSize = {.min = 0.45F, .max = 0.6F},
            .endSize = {.min = 0.35F, .max = 0.45F},
            .inherit = 0.5F,
            .launch = {.y = 16.0F},
            .spread = {.x = 10.0F, .y = 6.0F, .z = 10.0F},
            .acceleration = {.y = -50.0F},
            .drag = 0.3F,
            .streakSeconds = 0.05F,
            .color = color,
            .fadeInShare = 0.0F,
            .fadeOutShare = 0.3F};
}

constexpr FxEffect Cloud(std::string_view emitter, float rate, Color color, float size) {
    return {.emitter = emitter,
            .ratePerSecond = rate,
            .lifeMs = {.min = 600.0F, .max = 1100.0F},
            .startSize = {.min = size * 0.4F, .max = size * 0.6F},
            .endSize = {.min = size * 1.4F, .max = size * 2.0F},
            .spinPerSecond = {.min = 0.3F, .max = 1.2F},
            .inherit = 0.2F,
            .launch = {.y = 2.0F},
            .spread = {.x = 4.0F, .y = 2.0F, .z = 4.0F},
            .acceleration = {.y = 1.0F},
            .drag = 2.0F,
            .color = color,
            .shade = 0.6F};
}

constexpr FxEffect kRipples{.emitter = "wateremt",
                            .shape = FxShape::Ring,
                            .ratePerSecond = 5.0F,
                            .lifeMs = {.min = 600.0F, .max = 800.0F},
                            .startSize = {.min = 1.0F, .max = 1.5F},
                            .endSize = {.min = 6.0F, .max = 8.0F},
                            .color = {.r = 0.85F, .g = 0.95F, .b = 1.0F, .a = 0.4F},
                            .offset = {.y = 0.2F},
                            .fadeInShare = 0.0F,
                            .fadeOutShare = 0.8F};

constexpr Color kGrass{.r = 0.5F, .g = 0.85F, .b = 0.28F};
constexpr Color kGrassDark{.r = 0.3F, .g = 0.58F, .b = 0.15F};
constexpr Color kSand{.r = 0.9F, .g = 0.8F, .b = 0.58F};
constexpr Color kSandDark{.r = 0.75F, .g = 0.64F, .b = 0.44F};
constexpr Color kDirt{.r = 0.5F, .g = 0.38F, .b = 0.25F};
constexpr Color kDirtDark{.r = 0.36F, .g = 0.26F, .b = 0.17F};
constexpr Color kWater{.r = 0.85F, .g = 0.93F, .b = 1.0F, .a = 0.8F};
constexpr Color kSwamp{.r = 0.5F, .g = 0.55F, .b = 0.28F, .a = 0.9F};
constexpr Color kSnow{.r = 0.97F, .g = 0.98F, .b = 1.0F};

}

std::vector<render::FxEffect> SurfaceFxEffects() {
    return {Flecks("grassemt", 70.0F, kGrass, kGrassDark),
            Cloud("grassemt", 6.0F, {.r = 0.6F, .g = 0.62F, .b = 0.45F, .a = 0.35F}, 3.0F),
            Flecks("sandemt", 50.0F, kSand, kSandDark),
            Cloud("sandemt", 25.0F, {.r = kSand.r, .g = kSand.g, .b = kSand.b, .a = 0.5F}, 4.0F),
            Flecks("dirtemt", 40.0F, kDirt, kDirtDark),
            Cloud("dirtemt", 20.0F, {.r = 0.62F, .g = 0.5F, .b = 0.36F, .a = 0.5F}, 4.0F),
            Droplets("wateremt", 90.0F, kWater),
            Cloud("wateremt", 15.0F, {.a = 0.45F}, 3.5F),
            kRipples,
            Droplets("swampemt", 55.0F, kSwamp),
            Cloud("swampemt", 12.0F, {.r = 0.45F, .g = 0.48F, .b = 0.3F, .a = 0.5F}, 3.5F),
            Flecks("snowkick", 35.0F, kSnow, kSnow),
            Cloud("snowkick", 30.0F, {.a = 0.65F}, 4.5F)};
}

}
