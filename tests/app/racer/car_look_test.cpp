#include "app/racer/car_look.hpp"

#include <doctest/doctest.h>

#include <array>
#include <cstdint>
#include <vector>

#include "sim/powerup/warp_effect.hpp"

namespace rr::app {
namespace {

sim::EffectSnapshot Warp(std::size_t racer, sim::WarpPhase phase, float leftMs) {
    return {.kind = sim::PowerupKind::Warp,
            .use = {.racer = static_cast<std::uint8_t>(racer), .level = 3},
            .phase = static_cast<std::uint8_t>(phase),
            .phaseLeftMs = leftMs};
}

TEST_CASE("a color transform's shifts halve and its offsets add in 255ths") {
    const render::VertexColorTransform colors =
        ToVertexColors({.shifts = {1, 0, 2, 0}, .offsets = {0, 51, 0, 0}});

    CHECK(colors.scale.r == 0.5F);
    CHECK(colors.scale.g == 1.0F);
    CHECK(colors.scale.b == 0.25F);
    CHECK(colors.offset.r == 0.0F);
    CHECK(colors.offset.g == doctest::Approx(0.2F));
}

TEST_CASE("only racers starting a warp shrink, and only tinted racers change color") {
    sim::RenderSnapshot snapshot;
    snapshot.racers.resize(3);
    snapshot.racers[1].tint = 0;
    snapshot.effects = {Warp(2, sim::WarpPhase::Starting, 125.0F),
                        Warp(0, sim::WarpPhase::Active, 125.0F),
                        Warp(7, sim::WarpPhase::Starting, 125.0F)};
    const std::vector<assets::ColorTransformResource> tints{{.shifts = {1, 1, 1, 0}}};
    std::array<CarLook, 3> looks{};
    looks[0].scale = 0.5F;

    LookCars(snapshot, tints, looks);

    CHECK(looks[0].scale == 1.0F);
    CHECK(looks[0].colors.scale.r == 1.0F);
    CHECK(looks[1].colors.scale.r == 0.5F);
    CHECK(looks[2].scale == doctest::Approx(0.7071F).epsilon(0.001));
    CHECK(looks[2].colors.scale.r == 1.0F);
}

}
}
