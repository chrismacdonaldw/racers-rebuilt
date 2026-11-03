#include "app/environment_map.hpp"

#include <doctest/doctest.h>

namespace rr::app {
namespace {

constexpr float kQuarterTurn = 0.25015557F;

TEST_CASE("an unturned model reflects from the texture's corner") {
    const render::UvOffset offset = EnvironmentOffset({.x = 1.0F}, {.z = 1.0F});

    CHECK(offset.u == 0.0F);
    CHECK(offset.v == 0.0F);
}

TEST_CASE("a turned model steps across the texture by the original's table") {
    CHECK(EnvironmentOffset({.y = 2.0F}, {.z = 1.0F}).u == doctest::Approx(kQuarterTurn));
    CHECK(EnvironmentOffset({.y = -1.0F}, {.z = 1.0F}).u == doctest::Approx(1.0F - kQuarterTurn));
    CHECK(EnvironmentOffset({.x = -1.0F}, {.z = 1.0F}).u == doctest::Approx(0.5F));
}

TEST_CASE("a model rolled onto its side reflects from the middle row") {
    CHECK(EnvironmentOffset({.x = 1.0F}, {.y = 1.0F}).v == doctest::Approx(0.50031113F));
}

TEST_CASE("a model whose X axis points straight up reflects from the left edge's middle") {
    CHECK(EnvironmentOffset({.z = 1.0F}, {.x = -1.0F}) == render::UvOffset{.v = 0.5F});
}

}
}
