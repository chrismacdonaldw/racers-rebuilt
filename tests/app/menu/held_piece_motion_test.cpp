#include "app/menu/held_piece_motion.hpp"

#include <doctest/doctest.h>

#include <algorithm>

namespace rr::app {
namespace {

constexpr Milliseconds kTick{10};
constexpr float kRest = 2.0F;
constexpr float kTrailTop = 6.0F;
constexpr float kDropRest = 2.05F;

float HighestTrail(HeldPieceMotion& motion, int ticks) {
    float highest = 0.0F;
    for (int tick = 0; tick < ticks; ++tick) {
        motion.Tick(kTick, true, kRest);
        if (const auto trail = motion.Layers(false).trail) {
            highest = std::max(highest, trail->height);
        }
    }
    return highest;
}

int TicksUntil(HeldPieceMotion& motion, PieceMotionEnd end) {
    for (int tick = 1; tick < 1000; ++tick) {
        if (motion.Tick(kTick, true, kRest) == end) {
            return tick;
        }
    }
    return 0;
}

}

TEST_CASE("a piece that can't land pulses its ghost under it") {
    HeldPieceMotion motion;
    motion.Tick(Milliseconds{250}, false, kRest);

    const HeldPieceLayers layers = motion.Layers(false);

    CHECK(layers.solid == kHoverHeight);
    CHECK_FALSE(layers.trail.has_value());
    const PieceLayer ghost = layers.ghost.value_or(PieceLayer{});
    CHECK(ghost.height == kRest);
    CHECK(ghost.alpha == doctest::Approx(50.0F / 255.0F));
}

TEST_CASE("a piece that can land runs a trail up from the car and back") {
    HeldPieceMotion motion;

    CHECK(HighestTrail(motion, 60) == doctest::Approx(kTrailTop));
    CHECK(motion.Layers(false).ghost.value_or(PieceLayer{}).alpha ==
          doctest::Approx(150.0F / 255.0F));
    CHECK(HighestTrail(motion, 60) < kTrailTop);
}

TEST_CASE("seen from overhead only the landing ghost shows") {
    HeldPieceMotion motion;
    motion.Tick(kTick, true, kRest);

    const HeldPieceLayers layers = motion.Layers(true);

    CHECK_FALSE(layers.solid.has_value());
    CHECK_FALSE(layers.trail.has_value());
    CHECK(layers.ghost.value_or(PieceLayer{}).alpha == doctest::Approx(150.0F / 255.0F));
}

TEST_CASE("a dropped piece falls to where it was dropped before it's placed") {
    HeldPieceMotion motion;
    motion.Tick(kTick, true, 0.0F);
    motion.Drop(kDropRest);
    CHECK(motion.IsBusy());

    CHECK(TicksUntil(motion, PieceMotionEnd::Dropped) == 65);
    CHECK_FALSE(motion.IsBusy());
}

TEST_CASE("an undone piece lifts back to hover") {
    HeldPieceMotion motion;
    motion.Lift(kRest);
    motion.Tick(Milliseconds{500}, true, kRest);
    CHECK(motion.Layers(false).solid.value_or(0.0F) == doctest::Approx(kRest + 3.0F));
    CHECK_FALSE(motion.Layers(false).ghost.has_value());

    CHECK(TicksUntil(motion, PieceMotionEnd::Lifted) == 58);
}

}
