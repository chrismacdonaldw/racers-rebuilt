#include "app/racer/racer_visuals.hpp"

#include <doctest/doctest.h>

#include <cmath>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

#include "app/track_space.hpp"

namespace rr::app {
namespace {

constexpr float kFrameMs = 16.0F;

void CheckNear(const Vector3& actual, const Vector3& expected) {
    CHECK(actual.x == doctest::Approx(expected.x));
    CHECK(actual.y == doctest::Approx(expected.y));
    CHECK(actual.z == doctest::Approx(expected.z));
}

assets::AnimationPart Part(std::string name, std::uint32_t firstTrack) {
    return {.name = std::move(name),
            .firstTrack = firstTrack,
            .frameCount = 10,
            .loopFrames = 11,
            .framesPerSecond = 30,
            .velocity = {},
            .bounds = {}};
}

JointedAssets Wheels() {
    const assets::KeyRun turning{.firstValue = 0, .firstFrame = 0, .count = 2};
    return {
        .skeleton =
            {.nodes = {{.name = "root", .position = {}, .rotation = {}, .parent = {}},
                       {.name = "axle", .position = {.x = 64.0F}, .rotation = {}, .parent = 0}}},
        .animations = {.positionKeys = {},
                       .rotationKeys = {Quaternion{},
                                        FromAxisAngle({.y = 1.0F},
                                                      std::numbers::pi_v<float> / 2.0F)},
                       .keyFrames = {0, 10},
                       .tracks = {{}, {.rotations = turning, .positions = {}}, {}, {}},
                       .parts = {Part("frwrd", 0), Part("rvrse", 2)}},
    };
}

JointedAssets Driver() {
    return {
        .skeleton = {.nodes = {{.name = "alpelvis", .position = {}, .rotation = {}, .parent = {}}}},
        .animations = {.positionKeys = {{.y = 64.0F}},
                       .rotationKeys = {},
                       .keyFrames = {},
                       .tracks = {{}, {}, {.rotations = {}, .positions = {.count = 1}}},
                       .parts = {Part("default", 0), Part("steer-r", 1), Part("steer-l", 2)}},
    };
}

RacerAssets Racer() {
    RacerAssets racer{.files = {}, .models = {}, .wheels = Wheels(), .driver = Driver()};
    racer.files.chassis.seat = {.x = -2.0F, .z = 1.0F};
    racer.files.champion.mass = 4500.0F;
    return racer;
}

ModelDraws Model(std::vector<std::uint16_t> joints) {
    return {.draws = std::vector<render::MeshDraw>(joints.size()),
            .joints = std::move(joints),
            .scale = 1.0F / 64.0F};
}

RacerVisuals MakeVisuals(const RacerAssets& racer) {
    Result<RacerVisuals> visuals = RacerVisuals::Create(
        racer, {.wheels = Model({1}), .body = Model({0}), .driver = Model({0})});
    REQUIRE(visuals.has_value());
    return std::move(*visuals);
}

Vector3 GamePosition(const render::MeshDraw& draw) {
    const Vector3 engine = draw.transform.position;
    return {.x = engine.x, .y = -engine.z, .z = engine.y};
}

Vector3 GameAxis(const render::MeshDraw& draw, const Vector3& axis) {
    const Vector3 engine = Rotate(draw.transform.rotation, ToEngine(axis));
    return {.x = engine.x, .y = -engine.z, .z = engine.y};
}

TEST_CASE("a racer's body sits on its wheels and its driver in the seat") {
    const RacerAssets racer = Racer();
    RacerVisuals visuals = MakeVisuals(racer);

    visuals.Update({.position = {.x = 10.0F}}, {.id = {}, .forwardSpeed = 0.0F, .turnRadius = 0.0F},
                   kFrameMs);
    const std::span<const render::MeshDraw> draws = visuals.Draws();

    REQUIRE(draws.size() == 3);
    CheckNear(GamePosition(draws[0]), {.x = 11.0F});
    CheckNear(GamePosition(draws[1]), {.x = 10.0F});
    CheckNear(GamePosition(draws[2]), {.x = 8.0F, .z = 1.0F});
}

TEST_CASE("wheels turn with forward speed and stand still at rest") {
    const RacerAssets racer = Racer();
    RacerVisuals visuals = MakeVisuals(racer);

    visuals.Update({}, {.id = {}, .forwardSpeed = 0.0F, .turnRadius = 0.0F}, kFrameMs);
    CheckNear(GameAxis(visuals.Draws()[0], {.x = 1.0F}), {.x = 1.0F});
    visuals.Update({}, {.id = {}, .forwardSpeed = 0.1F, .turnRadius = 0.0F}, 20.0F);
    const Vector3 turned = GameAxis(visuals.Draws()[0], {.x = 1.0F});

    CHECK(turned.x < 0.95F);
    CHECK(turned.z < 0.0F);
}

TEST_CASE("the driver leans into a left turn over the steering blend") {
    const RacerAssets racer = Racer();
    RacerVisuals visuals = MakeVisuals(racer);
    const sim::RacerSnapshot left{.id = {}, .forwardSpeed = 0.0F, .turnRadius = 50.0F};

    visuals.Update({}, left, 150.0F);
    const Vector3 blending = GamePosition(visuals.Draws()[2]);
    visuals.Update({}, left, 150.0F);
    const Vector3 blended = GamePosition(visuals.Draws()[2]);

    CHECK(blending.y == doctest::Approx(0.5F));
    CHECK(blended.y == doctest::Approx(1.0F));
}

TEST_CASE("a draw that follows a node the skeleton lacks fails") {
    const RacerAssets racer = Racer();

    CHECK_FALSE(RacerVisuals::Create(
                    racer, {.wheels = Model({2}), .body = Model({0}), .driver = Model({0})})
                    .has_value());
}

TEST_CASE("a driver without steering parts fails") {
    RacerAssets racer = Racer();
    racer.driver.animations.parts.pop_back();

    CHECK_FALSE(RacerVisuals::Create(
                    racer, {.wheels = Model({1}), .body = Model({0}), .driver = Model({0})})
                    .has_value());
}

}
}
