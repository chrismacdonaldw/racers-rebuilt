#include "app/powerup/beam_span.hpp"

#include <doctest/doctest.h>

#include <vector>

#include "app/track_space.hpp"

namespace rr::app {
namespace {

constexpr BeamColours kColours{
    .base = {.b = 0.0F}, .outward = {.r = 0.0F, .b = 0.0F}, .inward = {.g = 0.0F, .b = 0.0F}};
constexpr Vector3 kTo{.x = 10.0F};
constexpr Vector3 kSide{.y = 1.0F};

TEST_CASE("a beam span bows out along its side and ends where it was asked to") {
    std::vector<RibbonPoint> path;

    AppendBeamSpan({}, kTo, kSide, 4.0F, kColours, path);

    REQUIRE(path.size() == kBeamSpanSteps);
    const Vector3 last = ToGame(path.back().position);
    CHECK(last.x == doctest::Approx(10.0F));
    CHECK(last.y == doctest::Approx(0.0F));
    CHECK(ToGame(path[1].position).y > 1.5F);
}

TEST_CASE("a beam span shades from its base colour toward the colour of its bulge") {
    std::vector<RibbonPoint> outward;
    std::vector<RibbonPoint> straight;

    AppendBeamSpan({}, kTo, kSide, 4.0F, kColours, outward);
    AppendBeamSpan({}, kTo, kSide, 0.0F, kColours, straight);

    CHECK(outward.back().tint.r == doctest::Approx(1.0F));
    CHECK(outward[1].tint.r == doctest::Approx(1.0F - (2.0F * 0.4F * 0.6F)));
    CHECK(outward[1].tint.g == doctest::Approx(1.0F));
    CHECK(straight[1].tint.g == doctest::Approx(1.0F - (2.0F * 0.4F * 0.6F)));
    CHECK(ToGame(straight[1].position).y == doctest::Approx(0.0F));
}

}
}
