#include "app/powerup/warp_start.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <vector>

namespace rr::app {
namespace {

sim::EffectSnapshot Warp(std::size_t racer, sim::WarpPhase phase, float leftMs) {
    return {.kind = sim::PowerupKind::Warp,
            .use = {.racer = static_cast<std::uint8_t>(racer), .level = 3},
            .phase = static_cast<std::uint8_t>(phase),
            .phaseLeftMs = leftMs};
}

TEST_CASE("only the racer's own warp in the asked phase is found") {
    const std::vector<sim::EffectSnapshot> effects{Warp(0, sim::WarpPhase::Starting, 400.0F),
                                                   Warp(1, sim::WarpPhase::Active, 900.0F),
                                                   {.kind = sim::PowerupKind::Turbo}};

    CHECK(FindWarp(effects, 0, sim::WarpPhase::Starting) == effects.data());
    CHECK(FindWarp(effects, 0, sim::WarpPhase::Active) == nullptr);
    CHECK(FindWarp(effects, 1, sim::WarpPhase::Active) == &effects[1]);
    CHECK(FindWarp(effects, 2, sim::WarpPhase::Starting) == nullptr);
}

TEST_CASE("the portal opens and closes over the start while the car shrinks only at the end") {
    CHECK(WarpPortalScale(500.0F) == doctest::Approx(0.0F));
    CHECK(WarpPortalScale(250.0F) == doctest::Approx(1.0F));
    CHECK(WarpPortalScale(125.0F) == doctest::Approx(0.7071F).epsilon(0.001));
    CHECK(WarpPortalScale(0.0F) == doctest::Approx(0.0F));
    CHECK(WarpCarScale(400.0F) == 1.0F);
    CHECK(WarpCarScale(125.0F) == doctest::Approx(0.7071F).epsilon(0.001));
}

TEST_CASE("only racers starting a warp get a car scale") {
    const std::vector<sim::EffectSnapshot> effects{Warp(2, sim::WarpPhase::Starting, 125.0F),
                                                   Warp(0, sim::WarpPhase::Active, 125.0F),
                                                   Warp(7, sim::WarpPhase::Starting, 125.0F)};
    std::array<float, 3> scales{0.5F, 0.5F, 0.5F};

    WarpCarScales(effects, scales);

    CHECK(scales[0] == 1.0F);
    CHECK(scales[1] == 1.0F);
    CHECK(scales[2] == doctest::Approx(0.7071F).epsilon(0.001));
}

TEST_CASE("the start eases the field of view 30 degrees wider") {
    const render::Camera base{.verticalFovDegrees = 60.0F};

    CHECK(WarpStartCamera(base, 500.0F).verticalFovDegrees == doctest::Approx(60.0F));
    CHECK(WarpStartCamera(base, 250.0F).verticalFovDegrees == doctest::Approx(75.0F));
    CHECK(WarpStartCamera(base, 0.0F).verticalFovDegrees == doctest::Approx(90.0F));
}

TEST_CASE("the flash fades in dark blue over the start's last 200 ms") {
    CHECK_FALSE(WarpFlash(200.0F).has_value());
    REQUIRE(WarpFlash(100.0F).has_value());
    const render::SpriteDraw half = WarpFlash(100.0F).value_or(render::SpriteDraw{});
    CHECK(half.tint.a == doctest::Approx(0.5F));
    CHECK(half.tint.b == doctest::Approx(100.0F / 255.0F));
    CHECK(half.tint.r == 0.0F);
    CHECK(half.bounds.x < 0.0F);
    CHECK(half.bounds.x + half.bounds.width > 640.0F);
}

}
}
