#include "app/racer/racer_visuals.hpp"

#include <doctest/doctest.h>

#include <numbers>
#include <span>
#include <utility>

#include "core/quaternion.hpp"
#include "core/transform.hpp"
#include "core/vector3.hpp"
#include "racer_rig.hpp"
#include "sim/race/random.hpp"
#include "sim/render_snapshot.hpp"

namespace rr::app {
namespace {

using test::CheckNear;
using test::Driver;
using test::GameAxis;
using test::GamePosition;
using test::Model;
using test::Wheels;

constexpr float kFrameMs = 16.0F;
constexpr float kQuarterTurn = std::numbers::pi_v<float> / 2.0F;

RacerAssets Racer() {
    RacerAssets racer{.files = {}, .models = {}, .wheels = Wheels(), .driver = Driver()};
    racer.files.chassis.seat = {.x = -2.0F, .z = 1.0F};
    racer.files.champion.mass = 4500.0F;
    return racer;
}

RacerVisuals MakeVisuals(const RacerAssets& racer) {
    Result<RacerVisuals> visuals = RacerVisuals::Create(
        racer, {.wheels = Model({1}), .body = Model({0}), .driver = Model({0})}, sim::Random(0));
    REQUIRE(visuals.has_value());
    return std::move(*visuals);
}

TEST_CASE("a racer's body sits on its wheels and its driver in the seat") {
    const RacerAssets racer = Racer();
    RacerVisuals visuals = MakeVisuals(racer);

    visuals.Update({.position = {.x = 10.0F}}, {.id = {}, .forwardSpeed = 0.0F, .turnRadius = 0.0F},
                   {}, kFrameMs);
    const std::span<const render::MeshDraw> draws = visuals.Draws();

    REQUIRE(draws.size() == 3);
    CheckNear(GamePosition(draws[0], 1), {.x = 11.0F});
    CheckNear(GamePosition(draws[1]), {.x = 10.0F});
    CheckNear(GamePosition(draws[2]), {.x = 8.0F, .z = 1.0F});
}

TEST_CASE("wheels turn with forward speed and stand still at rest") {
    const RacerAssets racer = Racer();
    RacerVisuals visuals = MakeVisuals(racer);

    visuals.Update({}, {.id = {}, .forwardSpeed = 0.0F, .turnRadius = 0.0F}, {}, kFrameMs);
    CheckNear(GameAxis(visuals.Draws()[0], {.x = 1.0F}, 1), {.x = 1.0F});
    visuals.Update({}, {.id = {}, .forwardSpeed = 0.1F, .turnRadius = 0.0F}, {}, 20.0F);
    const Vector3 turned = GameAxis(visuals.Draws()[0], {.x = 1.0F}, 1);

    CHECK(turned.x < 0.95F);
    CHECK(turned.z < 0.0F);
}

TEST_CASE("the driver leans into a left turn over the steering blend") {
    const RacerAssets racer = Racer();
    RacerVisuals visuals = MakeVisuals(racer);
    const sim::RacerSnapshot left{.id = {}, .forwardSpeed = 0.0F, .turnRadius = 50.0F};

    visuals.Update({}, left, {}, 150.0F);
    const Vector3 blending = GamePosition(visuals.Draws()[2]);
    visuals.Update({}, left, {}, 150.0F);
    const Vector3 blended = GamePosition(visuals.Draws()[2]);

    CHECK(blending.y == doctest::Approx(0.5F));
    CHECK(blended.y == doctest::Approx(1.0F));
}

TEST_CASE("the driver reacts to a hit once, then blends back into steering") {
    const RacerAssets racer = Racer();
    RacerVisuals visuals = MakeVisuals(racer);
    const sim::RacerSnapshot hit{
        .id = {}, .forwardSpeed = 0.0F, .turnRadius = 50.0F, .hitReactions = 1};

    visuals.Update({}, hit, {}, 100.0F);
    const Vector3 reacting = GamePosition(visuals.Draws()[2]);
    visuals.Update({}, hit, {}, 300.0F);
    visuals.Update({}, hit, {}, 150.0F);
    const Vector3 blending = GamePosition(visuals.Draws()[2]);
    visuals.Update({}, hit, {}, 150.0F);
    const Vector3 steering = GamePosition(visuals.Draws()[2]);

    CHECK(reacting.z == doctest::Approx(2.0F));
    CHECK(reacting.y == doctest::Approx(0.0F));
    CHECK(blending.z == doctest::Approx(1.5F));
    CHECK(blending.y == doctest::Approx(0.5F));
    CHECK(steering.z == doctest::Approx(1.0F));
    CHECK(steering.y == doctest::Approx(1.0F));
}

TEST_CASE("a sudden stop plays the stop part once, then the driver blends back") {
    const RacerAssets racer = Racer();
    RacerVisuals visuals = MakeVisuals(racer);
    const sim::RacerSnapshot stopped{.id = {}, .turnRadius = 50.0F, .suddenStops = 1};

    visuals.Update({}, stopped, {}, 100.0F);
    const Vector3 stopping = GamePosition(visuals.Draws()[2]);
    visuals.Update({}, stopped, {}, 300.0F);
    visuals.Update({}, stopped, {}, 300.0F);
    const Vector3 steering = GamePosition(visuals.Draws()[2]);

    CHECK(stopping.z == doctest::Approx(2.0F));
    CHECK(steering.y == doctest::Approx(1.0F));
}

TEST_CASE("the driver looks toward a racer alongside on its left, not its right") {
    const RacerAssets racer = Racer();
    RacerVisuals left = MakeVisuals(racer);
    RacerVisuals right = MakeVisuals(racer);
    const Transform facingY{.position = {}, .rotation = FromAxisAngle({.z = 1.0F}, kQuarterTurn)};

    for (RacerVisuals* visuals : {&left, &right}) {
        visuals->Update(facingY, {}, {}, 2000.0F);
    }
    left.Update(facingY, {}, Vector3{.x = -50.0F}, kFrameMs);
    right.Update(facingY, {}, Vector3{.x = 50.0F}, kFrameMs);

    CHECK(GamePosition(left.Draws()[2]).x == doctest::Approx(-1.0F));
    CHECK(GamePosition(right.Draws()[2]).x == doctest::Approx(0.0F));
}

TEST_CASE("a draw that follows a node the skeleton lacks fails") {
    const RacerAssets racer = Racer();

    CHECK_FALSE(
        RacerVisuals::Create(
            racer, {.wheels = Model({2}), .body = Model({0}), .driver = Model({0})}, sim::Random(0))
            .has_value());
}

TEST_CASE("a driver without steering parts fails") {
    RacerAssets racer = Racer();
    racer.driver.animations.parts.pop_back();

    CHECK_FALSE(
        RacerVisuals::Create(
            racer, {.wheels = Model({1}), .body = Model({0}), .driver = Model({0})}, sim::Random(0))
            .has_value());
}

}
}
