#include "app/racer/body_lean.hpp"

#include <doctest/doctest.h>

#include <cmath>

namespace rr::app {
namespace {

constexpr float kFrameMs = 16.0F;
constexpr float kMass = 4500.0F;
constexpr float kTilt = 0.2F;

float NosePitch(const Transform& pose) {
    return Rotate(pose.rotation, {.x = 1.0F}).z;
}

float SideRoll(const Transform& pose) {
    return Rotate(pose.rotation, {.y = 1.0F}).z;
}

Transform Pitched(float radians) {
    return {.position = {.x = 3.0F}, .rotation = FromAxisAngle({.y = -1.0F}, radians)};
}

TEST_CASE("a level car's body sits on its wheels") {
    BodyLean lean;

    const Transform body = lean.Update(Pitched(0.0F), kFrameMs, kMass);

    CHECK(body.position == Vector3{.x = 3.0F});
    CHECK(NosePitch(body) == doctest::Approx(0.0F));
    CHECK(SideRoll(body) == doctest::Approx(0.0F));
}

TEST_CASE("a body lags behind a sudden pitch of its wheels and then settles") {
    BodyLean lean;
    static_cast<void>(lean.Update(Pitched(0.0F), kFrameMs, kMass));

    const Transform first = lean.Update(Pitched(kTilt), kFrameMs, kMass);
    Transform settled = first;
    for (int frame = 0; frame < 600; ++frame) {
        settled = lean.Update(Pitched(kTilt), kFrameMs, kMass);
    }

    CHECK(NosePitch(first) < NosePitch(Pitched(kTilt)));
    CHECK(NosePitch(settled) == doctest::Approx(NosePitch(Pitched(kTilt))).epsilon(0.01));
}

TEST_CASE("a drift's roll raises the side it is signed toward") {
    BodyLean lean;
    static_cast<void>(lean.Update(Pitched(0.0F), kFrameMs, kMass));

    CHECK(SideRoll(lean.Update(Pitched(0.0F), kFrameMs, kMass, 0.05F)) > 0.0F);
    CHECK(SideRoll(BodyLean().Update(Pitched(0.0F), kFrameMs, kMass, -0.05F)) < 0.0F);
}

TEST_CASE("a heavier car's body follows its wheels more slowly") {
    BodyLean light;
    BodyLean heavy;
    static_cast<void>(light.Update(Pitched(0.0F), kFrameMs, kMass));
    static_cast<void>(heavy.Update(Pitched(0.0F), kFrameMs, kMass * 2.0F));

    const Transform lightBody = light.Update(Pitched(kTilt), kFrameMs, kMass);
    const Transform heavyBody = heavy.Update(Pitched(kTilt), kFrameMs, kMass * 2.0F);

    CHECK(NosePitch(heavyBody) < NosePitch(lightBody));
}

}
}
