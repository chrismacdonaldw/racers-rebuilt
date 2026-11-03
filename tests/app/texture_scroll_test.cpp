#include "app/texture_scroll.hpp"

#include <doctest/doctest.h>

#include <array>

namespace rr::app {
namespace {

TEST_CASE("a speed truncates to the original's fixed point") {
    const render::UvOffset offset = ScrolledBy({.uPerSecond = -0.5F, .vPerSecond = 0.01F}, 1000.0F);

    CHECK(offset.u == doctest::Approx(1.0F - 0.48828125F));
    CHECK(offset.v == 0.0F);
}

TEST_CASE("a scroll wraps to one texture width in either direction") {
    const render::UvOffset offset = ScrolledBy({.uPerSecond = 1.0F, .vPerSecond = -0.5F}, 1024.0F);

    CHECK(offset.u == doctest::Approx(0.015625F));
    CHECK(offset.v == doctest::Approx(0.5F));
}

TEST_CASE("a scroll stays exact after an hour") {
    const assets::TextureScroll scroll{.uPerSecond = 0.75F};

    CHECK(ScrolledBy(scroll, 3'604'480.0F).u == 0.0F);
    CHECK(ScrolledBy(scroll, 3'605'504.0F).u == doctest::Approx(0.765625F));
}

TEST_CASE("scrolled spans offset only their own draws") {
    std::array<render::MeshDraw, 4> draws{};
    const std::array scrolled{
        ScrolledDraws{.draws = {.first = 1, .count = 2}, .scroll = {.uPerSecond = 0.25F}}};

    ScrollDraws(scrolled, 1024.0F, draws);

    CHECK(draws[0].material.uvOffset == render::UvOffset{});
    CHECK(draws[1].material.uvOffset == render::UvOffset{.u = 0.25F});
    CHECK(draws[2].material.uvOffset == render::UvOffset{.u = 0.25F});
    CHECK(draws[3].material.uvOffset == render::UvOffset{});
}

}
}
