#include "sim/ai/ai_racer.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <numbers>

namespace rr::sim {
namespace {

constexpr Milliseconds kTick{12};

assets::RaceRoute Straight() {
    const Quaternion facingY = FromAxisAngle({.z = 1.0F}, std::numbers::pi_v<float> / 2.0F);
    const assets::RoutePoint point{
        .offset = {.x = 1.0F}, .rotation = facingY, .kind = 3, .durationMs = 100};
    return {.startPosition = {.y = 5.0F},
            .startRotation = facingY,
            .loopPosition = {.x = 1.0F, .y = 5.0F},
            .loopRotation = facingY,
            .loopTimeMs = 100,
            .loopPoint = 0,
            .points = {point, point, point, point, point}};
}

TEST_CASE("an ai racer starts at its route's start pose and plays it as recorded") {
    AiRacer racer(Straight());
    CHECK(racer.Pose().position == Vector3{.y = 5.0F});

    racer.Tick(Milliseconds{200});

    CHECK(racer.Pose().position.x == doctest::Approx(2.0F));
    CHECK(racer.Velocity().x == doctest::Approx(0.01F));
    CHECK(racer.Facing().y == doctest::Approx(1.0F));
    CHECK(racer.ContactCount() == 4);
}

TEST_CASE("an ai racer speeds up by 1 every 2048 ms") {
    AiRacer racer(Straight());
    racer.SetTargetSpeed(1.5F);

    racer.Tick(Milliseconds{512});
    CHECK(racer.PlaybackSpeed() == doctest::Approx(1.25F));

    racer.Tick(Milliseconds{2048});
    CHECK(racer.PlaybackSpeed() == 1.5F);
}

TEST_CASE("an ai racer slows down by 0.002 a ms") {
    AiRacer racer(Straight());
    racer.SetTargetSpeed(0.5F);

    racer.Tick(Milliseconds{100});
    CHECK(racer.PlaybackSpeed() == doctest::Approx(0.8F));

    racer.Tick(Milliseconds{500});
    CHECK(racer.PlaybackSpeed() == 0.5F);
}

TEST_CASE("an ai racer covers more route the faster it plays") {
    AiRacer slow(Straight());
    AiRacer fast(Straight());
    slow.SetTargetSpeed(0.95F);
    fast.SetTargetSpeed(1.05F);

    for (int tick = 0; tick < 25; ++tick) {
        slow.Tick(kTick);
        fast.Tick(kTick);
    }

    CHECK(slow.Pose().position.x < 3.0F);
    CHECK(fast.Pose().position.x > 3.0F);
}

TEST_CASE("an ai racer ignores an empty tick") {
    AiRacer racer(Straight());
    racer.SetTargetSpeed(2.0F);

    racer.Tick(Milliseconds{0});

    CHECK(racer.PlaybackSpeed() == 1.0F);
    CHECK(racer.Pose().position == Vector3{.y = 5.0F});
}

}
}
