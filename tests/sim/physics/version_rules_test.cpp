#include "sim/physics/version_rules.hpp"

#include <doctest/doctest.h>

#include <cmath>

#include "plane_world.hpp"
#include "sim/physics/body_effects.hpp"
#include "sim/physics/car_body.hpp"
#include "sim/physics/facing.hpp"
#include "sim/physics/ground_contact.hpp"
#include "sim/physics/wall_sweep.hpp"

namespace rr::sim {
namespace {

constexpr Vector3 kSlopeNormal{.x = -0.5F, .z = 0.8660254F};
constexpr float kRightAngle = 1.5707964F;

BodyState BodyFor(const BodyConstants& constants) {
    return MakeBody(constants, {}, {});
}

float TurnedAfter(const VersionRules& rules, int dtMs) {
    const BodyConstants constants = MakeBodyConstants({}, {}, rules);
    BodyState body = BodyFor(constants);
    body.facing = {.y = 1.0F};
    UpdateFacing(body, rules.facingRatePerMs, dtMs);
    return kRightAngle - std::acos(Dot(body.facing, body.motion.basis.forward));
}

BodyState HitSlope(const VersionRules& rules) {
    const BodyConstants constants = MakeBodyConstants({}, {}, rules);
    BodyState body = BodyFor(constants);
    body.motion.velocity = {.x = 0.1F, .z = -0.1F};
    ApplyWallResponse(body, constants, kSlopeNormal);
    return body;
}

BodyState HoverOverFloor(const BodyConstants& constants, float lift) {
    BodyState body = BodyFor(constants);
    StartHover(body, constants);
    body.hover.lift.height = lift;
    body.derived.isAtRest = false;
    PlaceWheels(body.motion, constants);
    body.motion.position.z += 0.5F - body.motion.wheels[0].position.z;
    PlaceWheels(body.motion, constants);
    return body;
}

TEST_CASE("1999 rules are the defaults") {
    const VersionRules& rules = RulesFor(assets::GameVersion::Release1999);
    const VersionRules defaults;

    CHECK(rules.steeringPerSecond.centring == defaults.steeringPerSecond.centring);
    CHECK(rules.facingRatePerMs == defaults.facingRatePerMs);
    CHECK_FALSE(rules.squaresAxis);
    CHECK_FALSE(rules.placesWheelsByShadow);
    CHECK_FALSE(rules.separatesFloorHits);
}

TEST_CASE("2001 swings the facing toward the nose 46 percent faster") {
    CHECK(TurnedAfter({}, 100) == doctest::Approx(0.25F).epsilon(0.01));
    CHECK(TurnedAfter(RulesFor(assets::GameVersion::Release2001), 100) ==
          doctest::Approx(0.3662109F).epsilon(0.01));
}

TEST_CASE("2001 holds a hovering car's wheels the lift height above the ground") {
    const BodyConstants constants =
        MakeBodyConstants({}, {}, RulesFor(assets::GameVersion::Release2001));
    BodyState body = HoverOverFloor(constants, 1.0F);
    const test::PlaneWorld floor = test::FlatGround();

    UpdateGroundContact(body, constants, 12, floor);

    CHECK(body.motion.contactCount == kWheelCount);
    for (const WheelProbe& wheel : body.motion.wheels) {
        CHECK(wheel.position.z == doctest::Approx(1.0F));
    }
}

TEST_CASE("2001 bounces off a gentle slope like a floor, without the wall yaw") {
    const BodyState original = HitSlope({});
    const BodyState rerelease = HitSlope(RulesFor(assets::GameVersion::Release2001));

    CHECK(original.hasWallYaw);
    CHECK(original.impulses.yawMs > 0);
    CHECK_FALSE(rerelease.hasWallYaw);
    CHECK(rerelease.impulses.yawMs == 0);
    CHECK(rerelease.motion.velocity.z > original.motion.velocity.z);
    CHECK(rerelease.motion.velocity.x == original.motion.velocity.x);
}

TEST_CASE("1999 rolls a drifting body by how far its facing trails, 2001 doesn't") {
    const BodyConstants constants =
        MakeBodyConstants({}, {}, RulesFor(assets::GameVersion::Release1999));
    BodyState body = BodyFor(constants);
    body.facing = {.y = 1.0F};
    body.turnRadius = -50.0F;

    CHECK(DriftRoll(body, true, RulesFor(assets::GameVersion::Release1999)) ==
          doctest::Approx(-0.06F));
    CHECK(DriftRoll(body, false, RulesFor(assets::GameVersion::Release1999)) == 0.0F);
    CHECK(DriftRoll(body, true, RulesFor(assets::GameVersion::Release2001)) == 0.0F);
}

}
}
