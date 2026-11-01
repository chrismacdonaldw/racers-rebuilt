#include "app/hazard/water_scroll.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>
#include <vector>

namespace rr::app {
namespace {

constexpr Milliseconds kTick{12};

std::vector<assets::Hazard> Water(float uAmplitude, float vAmplitude) {
    return {{.kind = assets::HazardKind::Ghost},
            {.kind = assets::HazardKind::WaterScroll,
             .payload = assets::WaterScroll{
                 .entity = "pswater", .uAmplitude = uAmplitude, .vAmplitude = vAmplitude}}};
}

std::vector<NamedDraws> Named() {
    return {{.entity = "rock", .span = {.first = 0, .count = 1}},
            {.entity = "PSWATER", .span = {.first = 1, .count = 2}}};
}

}

TEST_CASE("water slides each tick by the speed the tick before set") {
    const WaterScroll water(Water(0.3F, -0.03F), Named());
    WaterScroll scrolled = water;
    std::vector<render::MeshDraw> draws(3);

    scrolled.Tick(kTick);
    scrolled.Apply(draws);
    CHECK(draws[1].material.uvOffset.u == 0.0F);

    scrolled.Tick(kTick);
    scrolled.Apply(draws);

    const auto speed = static_cast<float>(static_cast<int>(
        0.3F * std::sin(2.0F * std::numbers::pi_v<float> * 1.0F / 1024.0F) * 65.536F));
    CHECK(draws[1].material.uvOffset.u == doctest::Approx(speed * 12.0F / 65'536.0F));
    CHECK(draws[2].material.uvOffset == draws[1].material.uvOffset);
    CHECK(draws[0].material.uvOffset.u == 0.0F);
}

TEST_CASE("water with a broken amplitude stays still or slides at the fastest speed") {
    std::vector<assets::Hazard> hazards = Water(std::numeric_limits<float>::quiet_NaN(), 1e30F);
    WaterScroll water(hazards, Named());
    std::vector<render::MeshDraw> draws(3);

    water.Tick(kTick);
    water.Tick(kTick);
    water.Apply(draws);

    CHECK(draws[1].material.uvOffset.u == 0.0F);
    CHECK(draws[1].material.uvOffset.v == doctest::Approx(((32'767 * 12) % 65'536) / 65'536.0F));
}

TEST_CASE("water drifts out and back over a 10 s period") {
    WaterScroll water(Water(0.3F, 0.03F), Named());
    std::vector<render::MeshDraw> draws(3);
    float highest = 0.0F;

    for (int tick = 0; tick < 10'000 / 12; ++tick) {
        water.Tick(kTick);
        water.Apply(draws);
        highest = std::max(highest, draws[1].material.uvOffset.u);
    }

    CHECK(highest > 0.85F);
    const float back = draws[1].material.uvOffset.u;
    CHECK(std::min(back, 1.0F - back) < 0.05F);
}

}
