#include "app/racer/driver_moves.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <optional>

#include "core/vector3.hpp"
#include "sim/race/random.hpp"

namespace rr::app {
namespace {

constexpr float kFrameMs = 16.0F;
constexpr float kPastFirstLookMs = 2000.0F;

DriverCue Once(DriverPart part, DriverPart then = DriverPart::Default) {
    return {.part = part, .kind = DriverCueKind::Once, .then = then};
}

DriverCue Loop(DriverPart part) {
    return {.part = part, .kind = DriverCueKind::Loop};
}

DriverCue Blend(DriverPart part) {
    return {.part = part, .kind = DriverCueKind::Blend};
}

DriverMoves Ready() {
    DriverMoves moves{sim::Random(1)};
    CHECK_FALSE(moves.Next({}, false, kPastFirstLookMs).has_value());
    return moves;
}

std::optional<DriverCue> LookAt(const Vector3& nearest) {
    DriverMoves moves = Ready();
    return moves.Next({.nearestRacer = nearest}, false, kFrameMs);
}

TEST_CASE("the driver blends to the steering part on the turn's side") {
    DriverMoves moves = Ready();

    CHECK(moves.Next({.turnRadius = -30.0F}, false, kFrameMs) == Blend(DriverPart::SteerRight));
    CHECK_FALSE(moves.Next({.turnRadius = -10.0F}, false, kFrameMs).has_value());
    CHECK(moves.Next({.turnRadius = 30.0F}, false, kFrameMs) == Blend(DriverPart::SteerLeft));
    CHECK(moves.Next({}, false, kFrameMs) == Blend(DriverPart::Default));
}

TEST_CASE("a sudden stop plays once and holds off steering until it ends") {
    DriverMoves moves = Ready();

    CHECK(moves.Next({.hasStopped = true}, false, kFrameMs) == Once(DriverPart::SuddenStop));
    CHECK_FALSE(moves.Next({.turnRadius = 30.0F, .hasStopped = true}, true, kFrameMs).has_value());
    CHECK(moves.Next({.turnRadius = 30.0F}, false, kFrameMs) == Blend(DriverPart::SteerLeft));
}

TEST_CASE("a hit cuts into a stop, and a part that ends settles into what follows it") {
    DriverMoves moves = Ready();

    CHECK(moves.Next({.hasStopped = true}, false, kFrameMs) == Once(DriverPart::SuddenStop));
    CHECK(moves.Next({.wasHit = true}, true, kFrameMs) == Once(DriverPart::Hit));
    CHECK_FALSE(moves.Next({.isReversing = true}, true, kFrameMs).has_value());
    CHECK(moves.Next({}, false, kFrameMs) == Loop(DriverPart::Default));
}

TEST_CASE("reversing plays into a held pose, and stopping plays the way back") {
    DriverMoves moves = Ready();

    CHECK(moves.Next({.isReversing = true}, false, kFrameMs) ==
          Once(DriverPart::Reverse, DriverPart::ReverseHold));
    CHECK_FALSE(moves.Next({.isReversing = true}, true, kFrameMs).has_value());
    CHECK(moves.Next({.isReversing = true}, false, kFrameMs) == Loop(DriverPart::ReverseHold));
    CHECK(moves.Next({}, false, kFrameMs) == Once(DriverPart::ReverseBack));
}

TEST_CASE("the driver looks toward a racer beside or behind it, on its side") {
    CHECK(LookAt({.x = 0.1F, .y = 0.9F}) == Once(DriverPart::SideLookLeft));
    CHECK(LookAt({.x = -0.1F, .y = -0.9F}) == Once(DriverPart::SideLookRight));
    CHECK(LookAt({.x = -0.5F, .y = 0.8F}) == Once(DriverPart::RearLookLeft));
    CHECK(LookAt({.x = -0.2F, .y = -0.9F}) == Once(DriverPart::RearLookRight));
    CHECK_FALSE(LookAt({.x = 0.2F, .y = 0.9F}).has_value());
    CHECK_FALSE(LookAt({.x = -0.6F, .y = 0.8F}).has_value());
}

TEST_CASE("after a look the driver waits 5 to 6 s before the next") {
    DriverMoves moves = Ready();
    const DriverSense beside{.nearestRacer = Vector3{.y = 1.0F}};

    CHECK(moves.Next(beside, false, kFrameMs) == Once(DriverPart::SideLookLeft));
    CHECK_FALSE(moves.Next(beside, true, kFrameMs).has_value());
    CHECK(moves.Next(beside, false, 4900.0F) == Loop(DriverPart::Default));
    CHECK(moves.Next(beside, false, 1100.0F) == Once(DriverPart::SideLookLeft));
}

TEST_CASE("a racer that finishes behind the winner plays the loser parts and nothing else") {
    DriverMoves moves = Ready();
    const DriverSense finished{.wasHit = true, .finishPlace = 2};

    CHECK(moves.Next(finished, false, kFrameMs) == Once(DriverPart::Loser, DriverPart::LoserLoop));
    CHECK_FALSE(moves.Next(finished, true, kFrameMs).has_value());
    CHECK(moves.Next(finished, false, kFrameMs) == Loop(DriverPart::LoserLoop));
    CHECK_FALSE(moves.Next(finished, false, kFrameMs).has_value());
}

std::optional<std::uint64_t> SeedFor(const DriverCue& celebration) {
    for (std::uint64_t seed = 0; seed < 16; ++seed) {
        DriverMoves moves{sim::Random(seed)};
        if (moves.Next({.finishPlace = 1}, false, kFrameMs) == celebration) {
            return seed;
        }
    }
    return std::nullopt;
}

TEST_CASE("the winner plays one of its two celebrations and keeps it") {
    const std::optional<std::uint64_t> looping = SeedFor(Loop(DriverPart::Winner));
    const std::optional<std::uint64_t> starting =
        SeedFor(Once(DriverPart::WinnerStart, DriverPart::WinnerLoop));
    REQUIRE(looping.has_value());
    REQUIRE(starting.has_value());
    DriverMoves moves{sim::Random(*looping)};

    CHECK(moves.Next({.finishPlace = 1}, false, kFrameMs) == Loop(DriverPart::Winner));
    CHECK_FALSE(moves.Next({.wasHit = true, .finishPlace = 1}, false, kFrameMs).has_value());
}

}
}
