#include <doctest/doctest.h>

#include <cmath>
#include <utility>
#include <vector>

#include "../physics/plane_world.hpp"
#include "core/quaternion.hpp"
#include "sim/ai/ai_racer.hpp"
#include "sim/physics/racer.hpp"
#include "straight_route.hpp"

namespace rr::sim {
namespace {

constexpr Milliseconds kTick{12};
constexpr ChassisSpec kNeutralChassis{.handling = 50, .topSpeed = 50, .acceleration = 50};

void Play(AiRacer& racer, int durationMs) {
    for (int elapsed = 0; elapsed < durationMs; elapsed += 12) {
        racer.Tick(kTick);
    }
}

assets::RaceRoute LeftCurve() {
    constexpr float kYawPerPoint = 0.05F;
    std::vector<assets::RoutePoint> points;
    for (int i = 1; i <= 60; ++i) {
        const float yaw = kYawPerPoint * static_cast<float>(i);
        points.push_back({.offset = {.x = std::cos(yaw), .y = std::sin(yaw)},
                          .rotation = FromAxisAngle({.z = 1.0F}, yaw),
                          .kind = 3,
                          .durationMs = 100});
    }
    return {.startPosition = {},
            .loopPosition = points.front().offset,
            .loopTimeMs = 100,
            .loopPoint = 0,
            .points = std::move(points)};
}

AiRacer OnStraight() {
    return AiRacer(test::StraightRoute({}));
}

TEST_CASE("an ai spin of one turn at 0.01 runs 628 ms at 0.3 speed and ends on tick 53") {
    AiRacer racer = OnStraight();

    racer.StartSpin(1.0F, 0.01F);
    CHECK(racer.Effects().spin.remainingMs == 628);
    CHECK(racer.PlaybackSpeed() == 0.3F);
    Play(racer, 26 * 12);
    CHECK(racer.Facing().x < -0.99F);
    Play(racer, 26 * 12);
    CHECK(racer.Effects().isSpinning);
    CHECK(racer.PlaybackSpeed() == 0.3F);
    racer.Tick(kTick);

    CHECK_FALSE(racer.Effects().isSpinning);
    CHECK(racer.Facing().x == 1.0F);
}

TEST_CASE("an ai spin-out hops at 50 against a gravity of 100 and lands about a second later") {
    AiRacer racer = OnStraight();

    racer.StartSpinOut();
    CHECK(racer.PlaybackSpeed() == 0.1F);
    Play(racer, 492);
    CHECK(racer.Effects().isSpunOut);
    CHECK(racer.Pose().position.z == doctest::Approx(12.2016F));
    Play(racer, 600);

    CHECK_FALSE(racer.Effects().isSpunOut);
    CHECK(racer.Pose().position.z == 0.0F);
}

TEST_CASE("a hovering ai racer rises to 6 at 3 a second and drops at 15 a second") {
    AiRacer racer = OnStraight();

    racer.StartHover();
    Play(racer, 996);
    CHECK(racer.Effects().lift.height == doctest::Approx(2.988F));
    Play(racer, 1200);
    CHECK(racer.Effects().lift.height == 6.0F);
    CHECK(racer.Pose().position.z == doctest::Approx(6.0F));
    racer.StopHover();
    Play(racer, 204);
    CHECK(racer.Effects().lift.height == doctest::Approx(2.94F));
    Play(racer, 204);

    CHECK(racer.Effects().lift.height == 0.0F);
}

TEST_CASE("a hovering ai racer lifts its nose for 150 ms and settles back level") {
    AiRacer racer(test::StraightRoute({}), {.x = 2.0F, .z = -0.5F});

    racer.StartHover();
    Play(racer, 156);
    CHECK(racer.Effects().tilt.remainingMs == 0);
    CHECK(racer.Facing().z > 0.3F);
    CHECK(Rotate(racer.Pose().rotation, {.y = 1.0F}).y == doctest::Approx(1.0F));
    CHECK(racer.Effects().tilt.height > 0.5F);
    Play(racer, 1200);

    CHECK(racer.Effects().tilt.angle == 0.0F);
    CHECK(racer.Effects().tilt.height == 0.0F);
}

TEST_CASE("a hovering ai racer leans into a left turn") {
    AiRacer racer(LeftCurve());

    racer.StartHover();
    Play(racer, 1200);

    const Vector3 facing = racer.Facing();
    const Vector3 left{.x = -facing.y, .y = facing.x};
    CHECK(Dot(Rotate(racer.Pose().rotation, {.z = 1.0F}), left) > 0.6F);
}

TEST_CASE("a cursed ai racer holds half speed on the ground until the curse lifts") {
    AiRacer racer = OnStraight();

    racer.StartCurseSlow();
    Play(racer, 1200);
    CHECK(racer.PlaybackSpeed() == 0.5F);
    racer.EndCurseSlow();
    Play(racer, 600);

    CHECK(racer.PlaybackSpeed() == doctest::Approx(0.5F + (600.0F / 2048.0F)));
}

TEST_CASE("a push along an ai racer's facing raises its target by a third per 100 of force") {
    AiRacer racer = OnStraight();

    racer.StartPush({.x = 300.0F});
    CHECK(racer.TargetSpeed() == 2.0F);
    Play(racer, 252);
    CHECK(racer.PlaybackSpeed() == doctest::Approx(1.504F));
    racer.StartPush({.x = -3000.0F});
    CHECK(racer.TargetSpeed() == -0.5F);
    racer.EndPush();

    CHECK(racer.TargetSpeed() == 1.0F);
}

TEST_CASE("a spinning ai racer pushed either way swings 0.1 around its spin speed") {
    AiRacer racer = OnStraight();
    racer.StartSpin(2.0F, 0.007F);

    racer.StartPush({.x = 300.0F});
    CHECK(racer.TargetSpeed() == doctest::Approx(0.4F));
    racer.StartPush({.x = -300.0F});
    CHECK(racer.TargetSpeed() == doctest::Approx(0.2F));
    racer.EndPush();

    CHECK(racer.TargetSpeed() == 0.3F);
}

TEST_CASE("a ghost ai racer plays at 4.5 until it leaves ghost mode") {
    AiRacer racer = OnStraight();

    racer.StartGhost();
    Play(racer, 1200);
    CHECK(racer.PlaybackSpeed() == 4.5F);
    CHECK(racer.Effects().lift.height == doctest::Approx(3.6F));
    racer.EndGhost();

    CHECK(racer.PlaybackSpeed() == 1.0F);
}

TEST_CASE("a magnet stops an ai racer and pacing can't restart it until released") {
    const test::PlaneWorld world = test::FlatGround();
    Racer racer(kNeutralChassis, {}, {});
    racer.FollowRoute(test::StraightRoute({}));

    racer.StartMagnetHold();
    racer.SetRouteSpeed(1.5F);
    for (int tick = 0; tick < 50; ++tick) {
        racer.Tick(kTick, {}, world);
    }
    CHECK(racer.Route().transform(&AiRacer::TargetSpeed) == 0.0F);
    CHECK(racer.Route().transform(&AiRacer::PlaybackSpeed) == 0.0F);
    racer.EndMagnetHold();

    CHECK(racer.Route().transform(&AiRacer::TargetSpeed) == 1.0F);
}

TEST_CASE("a grabbed ai racer backs down its route until let go") {
    const test::PlaneWorld world = test::FlatGround();
    Racer racer(kNeutralChassis, {}, {});
    racer.FollowRoute(test::StraightRoute({}));

    racer.StartGrab({.x = 800.0F});
    for (int tick = 0; tick < 100; ++tick) {
        racer.Tick(kTick, {}, world);
    }
    CHECK(racer.Route().transform(&AiRacer::PlaybackSpeed) == doctest::Approx(-0.4F));
    CHECK_FALSE(racer.Body().externalForces[1].has_value());
    racer.EndGrab();

    CHECK(racer.Route().transform(&AiRacer::TargetSpeed) == 1.0F);
}

TEST_CASE("a cursed racer on a route is slowed until the tick its curse runs out") {
    const test::PlaneWorld world = test::FlatGround();
    Racer racer(kNeutralChassis, {}, {});
    racer.FollowRoute(test::StraightRoute({}));
    const auto isCurseSlowed = [](const AiRacer& route) { return route.Effects().isCurseSlowed; };

    racer.AttachCurse(Milliseconds{10'000});
    int ticks = 0;
    while (racer.Status().isCursed) {
        racer.Tick(kTick, {}, world);
        racer.UpdateTimers(kTick);
        CHECK(racer.Route().transform(isCurseSlowed) == racer.Status().isCursed);
        ++ticks;
    }

    CHECK(ticks == 834);
    CHECK(racer.Route().transform(&AiRacer::TargetSpeed) == 1.0F);
}

}
}
