#include "app/fov_swing.hpp"

#include <doctest/doctest.h>

#include <algorithm>

namespace rr::app {
namespace {

constexpr Milliseconds kTick{12};
constexpr float kBase = 60.0F;

struct Range {
    float lowest = kBase;
    float highest = kBase;
};

Range Swing(FovSwing& swing, int durationMs, bool isCursed, bool isGhost = false) {
    Range range;
    for (int elapsed = 0; elapsed < durationMs; elapsed += static_cast<int>(kTick.count())) {
        swing.Tick(kTick, isCursed, isGhost);
        range.lowest = std::min(range.lowest, swing.Degrees());
        range.highest = std::max(range.highest, swing.Degrees());
    }
    return range;
}

TEST_CASE("an uncursed camera keeps its base field of view") {
    FovSwing swing{kBase, sim::Random(1)};

    const Range range = Swing(swing, 2000, false);

    CHECK(range.lowest == kBase);
    CHECK(range.highest == kBase);
}

TEST_CASE("a curse swings the field of view within 40 to 80 degrees") {
    FovSwing swing{kBase, sim::Random(1)};

    const Range range = Swing(swing, 10'000, true);

    CHECK(range.lowest >= 40.0F);
    CHECK(range.highest <= 80.0F);
    CHECK(range.highest - range.lowest > 5.0F);
}

TEST_CASE("once the curse ends the field of view eases back to base") {
    FovSwing swing{kBase, sim::Random(1)};
    Swing(swing, 1250, true);
    REQUIRE(swing.Degrees() != kBase);

    Swing(swing, 260, false);
    const float easing = swing.Degrees();
    Swing(swing, 500, false);

    CHECK(easing != kBase);
    CHECK(swing.Degrees() == doctest::Approx(kBase).epsilon(0.001));
}

TEST_CASE("a ghost's camera snaps straight back to base") {
    FovSwing swing{kBase, sim::Random(1)};
    Swing(swing, 1250, true);

    swing.Tick(kTick, true, true);

    CHECK(swing.Degrees() == kBase);
}

}
}
