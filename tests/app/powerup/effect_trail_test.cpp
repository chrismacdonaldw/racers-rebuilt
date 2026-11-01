#include "app/powerup/effect_trail.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <vector>

#include "app/track_space.hpp"

namespace rr::app {
namespace {

constexpr TrailStyle kStyle{.width = 2.0F, .lifeMs = 400.0F, .colour = {.a = 0.5F}};
constexpr QuadLook kLook;
constexpr std::size_t kSidesPerSegment = 4;

std::vector<render::QuadDraw> Drawn(const EffectTrail& trail) {
    std::vector<render::QuadDraw> quads;
    trail.Append(kLook, kStyle, quads);
    return quads;
}

float HighestGameZ(const std::vector<render::QuadDraw>& quads, std::size_t first) {
    float highest = -1.0F;
    for (std::size_t i = first; i < first + kSidesPerSegment; ++i) {
        for (const Vector3& corner : quads[i].corners) {
            highest = std::max(highest, ToGame(corner).z);
        }
    }
    return highest;
}

float LowestGameX(const std::vector<render::QuadDraw>& quads) {
    float lowest = 1'000'000.0F;
    for (const render::QuadDraw& quad : quads) {
        for (const Vector3& corner : quad.corners) {
            lowest = std::min(lowest, ToGame(corner).x);
        }
    }
    return lowest;
}

TEST_CASE("a trail grows a square tube behind its head, one segment per quarter life") {
    EffectTrail trail;

    for (int frame = 0; frame <= 8; ++frame) {
        trail.Follow({.x = 10.0F * static_cast<float>(frame)}, kStyle, 16.0F);
    }

    const std::vector<render::QuadDraw> quads = Drawn(trail);
    REQUIRE(quads.size() == 2 * kSidesPerSegment);
    const float headWidth = HighestGameZ(quads, kSidesPerSegment);
    const float tailWidth = HighestGameZ(quads, 0);
    CHECK(quads[0].tint.a == doctest::Approx(0.5F));
    CHECK(headWidth == doctest::Approx(1.0F));
    CHECK(tailWidth < 1.0F);
}

TEST_CASE("a trail narrows toward its tail and drops points older than its life") {
    EffectTrail trail;
    for (int frame = 0; frame <= 40; ++frame) {
        trail.Follow({.x = 10.0F * static_cast<float>(frame)}, kStyle, 20.0F);
    }

    const std::vector<render::QuadDraw> quads = Drawn(trail);
    REQUIRE(!quads.empty());
    CHECK(LowestGameX(quads) >= 199.0F);
    const float tail = ToGame(quads.front().corners[0]).z;
    CHECK(tail < 1.0F);
    CHECK(tail > 0.1F);
}

TEST_CASE("a cleared trail draws nothing") {
    EffectTrail trail;
    for (int frame = 0; frame <= 10; ++frame) {
        trail.Follow({.y = static_cast<float>(frame)}, kStyle, 50.0F);
    }

    trail.Clear();

    CHECK(Drawn(trail).empty());
}

}
}
