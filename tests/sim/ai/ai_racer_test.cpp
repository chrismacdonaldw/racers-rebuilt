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

assets::RaceRoute Lane() {
    const assets::RoutePoint point{
        .offset = {.x = 1.0F}, .widthLeft = 3.0F, .widthRight = 2.0F, .kind = 3, .durationMs = 100};
    return {.loopPosition = {.x = 1.0F},
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

TEST_CASE("a contact's push moves an ai racer sideways within the route's widths") {
    AiRacer racer(Lane());

    racer.Shift({.y = 1.5F});
    CHECK(racer.SideOffset() == 1.5F);
    CHECK(racer.Pose().position.y == doctest::Approx(1.5F));

    racer.Shift({.y = 5.0F});
    CHECK(racer.SideOffset() == 2.0F);
    racer.Shift({.y = -5.0F});
    CHECK(racer.SideOffset() == -3.0F);
}

TEST_CASE("a contact's push replaces the side offset and seeks along the route") {
    AiRacer racer(Lane());
    racer.Shift({.y = 1.0F});

    racer.Shift({.x = 0.5F});

    CHECK(racer.SideOffset() == 0.0F);
    CHECK(racer.Pose().position.x == doctest::Approx(0.5F));
}

TEST_CASE("a contact's push from the front seeks back along the route") {
    AiRacer racer(Lane());
    racer.Tick(Milliseconds{150});

    racer.Shift({.x = -0.8F});

    CHECK(racer.Pose().position.x == doctest::Approx(0.7F));
}

TEST_CASE("an ai racer drifts back to its line by 1 unit every 512 ms") {
    AiRacer racer(Lane());
    racer.Shift({.y = 2.0F});

    racer.Tick(Milliseconds{256});
    CHECK(racer.SideOffset() == doctest::Approx(1.5F));
    racer.Tick(Milliseconds{2000});
    CHECK(racer.SideOffset() == 0.0F);
}

float BumpedSpeed(const Vector3& direction, float impulse) {
    AiRacer racer(Lane());
    racer.Bump(direction, impulse);
    return racer.PlaybackSpeed();
}

TEST_CASE("a contact's impulse along an ai racer's heading speeds it up") {
    CHECK(BumpedSpeed({.x = 1.0F}, 120.0F) == doctest::Approx(1.5F));
    CHECK(BumpedSpeed({.y = 1.0F}, 480.0F) == doctest::Approx(2.05F));
    CHECK(BumpedSpeed({.x = 1.0F}, 1000.0F) == doctest::Approx(2.0F));
}

TEST_CASE("a contact's impulse against an ai racer's heading slows it down") {
    CHECK(BumpedSpeed({.x = -1.0F}, 120.0F) == doctest::Approx(0.5F));
    CHECK(BumpedSpeed({.x = 1.0F}, -120.0F) == doctest::Approx(0.5F));
    CHECK(BumpedSpeed({.x = -1.0F}, 1000.0F) == doctest::Approx(0.0F));
}

TEST_CASE("an ai racer's playback stays within -0.5 and 2.75 after impulses") {
    AiRacer racer(Lane());
    racer.Bump({.x = 1.0F}, 240.0F);
    racer.Bump({.x = 1.0F}, 240.0F);
    CHECK(racer.PlaybackSpeed() == 2.75F);

    for (int i = 0; i < 4; ++i) {
        racer.Bump({.x = -1.0F}, 240.0F);
    }
    CHECK(racer.PlaybackSpeed() == -0.5F);
}

TEST_CASE("a spinning ai racer takes an impulse from its spin speed") {
    AiRacer racer(Lane());
    racer.StartSpin(1.0F, 0.009F);

    racer.Bump({.x = 1.0F}, 10.0F);

    CHECK(racer.PlaybackSpeed() == doctest::Approx(0.4F));
}

}
}
