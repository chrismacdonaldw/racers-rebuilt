#include "sim/powerup/ballistic_flight.hpp"

#include <doctest/doctest.h>

#include "../physics/plane_world.hpp"
#include "powerup_rig.hpp"

namespace rr::sim {
namespace {

constexpr BallisticSpec kCannonball{
    .speed = 180.0F, .gravity = -32.176F, .lifetime = Milliseconds{3000}};
constexpr Vector3 kStart{.z = 5.0F};
constexpr Vector3 kAim{.x = 500.0F, .z = 5.0F};

Racer ParkedAt(const Vector3& position) {
    return {test::kChassis, CarBuild{}, RacerStart{.position = position}};
}

void CheckAt(BallisticFlight& flight, const test::PlaneWorld& world, Milliseconds step, float x,
             float z) {
    CAPTURE(x);
    REQUIRE(flight.Step(step, world) == FlightState::Flying);
    CHECK(flight.Position().x == doctest::Approx(x).epsilon(0.001));
    CHECK(flight.Position().y == doctest::Approx(0.0F));
    CHECK(flight.Position().z == doctest::Approx(z).epsilon(0.001));
}

TEST_CASE("a cannonball with no target follows the spec's worked arc") {
    const Racer owner = ParkedAt({});
    const test::PlaneWorld nothing({});
    BallisticFlight flight;
    flight.LaunchAtPoint(kCannonball, owner, kStart, kAim);

    CHECK(flight.Speed() == doctest::Approx(180.0F));
    CHECK(flight.Velocity().z == doctest::Approx(44.689F).epsilon(0.0005));
    CheckAt(flight, nothing, Milliseconds{500}, 90.0F, 23.32F);
    CheckAt(flight, nothing, Milliseconds{500}, 180.0F, 33.60F);
    CheckAt(flight, nothing, Milliseconds{1000}, 360.0F, 30.03F);
    CheckAt(flight, nothing, Milliseconds{777}, 499.86F, 5.03F);
    CHECK(flight.Step(Milliseconds{222}, nothing) == FlightState::Flying);
    CHECK(flight.Position().z < 0.0F);
    CHECK(flight.Step(Milliseconds{1}, nothing) == FlightState::Expired);
}

TEST_CASE("a cannonball over flat ground lands 519.4 ahead at 2.885 s") {
    const Racer owner = ParkedAt({});
    const test::PlaneWorld ground = test::FlatGround();
    BallisticFlight flight;
    flight.LaunchAtPoint(kCannonball, owner, kStart, kAim);

    int ticks = 1;
    while (flight.Step(test::kTick, ground) == FlightState::Flying) {
        ++ticks;
    }
    CHECK(ticks == 241);
    CHECK(flight.Position().x == doctest::Approx(519.4F).epsilon(0.001));
    CHECK(flight.Position().z == doctest::Approx(0.0F));
    CHECK(flight.HitNormal().z == doctest::Approx(1.0F));
}

TEST_CASE("a shot at a racer reaches its aim point after the flight time") {
    const Racer owner = ParkedAt({});
    const Racer target = ParkedAt({.x = 360.0F});
    const test::PlaneWorld nothing({});
    BallisticFlight flight;
    flight.LaunchAtRacer(kCannonball, owner, kStart, target);

    REQUIRE(flight.Step(Milliseconds{2000}, nothing) == FlightState::Flying);
    CHECK(flight.Position().x == doctest::Approx(360.0F).epsilon(0.001));
    CHECK(flight.Position().z == doctest::Approx(AimPoint(target).z).epsilon(0.001));
}

TEST_CASE("an owner driving toward the aim adds its speed, and away from it adds nothing") {
    Racer owner = ParkedAt({.z = 0.2F});
    const test::PlaneWorld ground = test::FlatGround();
    for (int tick = 0; tick < 200; ++tick) {
        owner.Tick(test::kTick, test::kThrottle, ground);
    }
    const float ownerSpeed = owner.Body().derived.speed * 1000.0F;
    REQUIRE(ownerSpeed > 10.0F);
    const Vector3 at = owner.Body().motion.position;

    BallisticFlight ahead;
    ahead.LaunchAtPoint(kCannonball, owner, at, at + Vector3{.x = 500.0F});
    CHECK(ahead.Speed() == doctest::Approx(180.0F + ownerSpeed).epsilon(0.01));

    BallisticFlight behind;
    behind.LaunchAtPoint(kCannonball, owner, at, at - Vector3{.x = 500.0F});
    CHECK(behind.Speed() == doctest::Approx(180.0F));
}

TEST_CASE("a shot turned back heads for its old owner from where it was") {
    const Racer owner = ParkedAt({});
    const test::PlaneWorld nothing({});
    BallisticFlight flight;
    flight.LaunchAtPoint(kCannonball, owner, kStart, kAim);
    REQUIRE(flight.Step(Milliseconds{1000}, nothing) == FlightState::Flying);
    const Vector3 turnedAt = flight.Position();

    flight.TurnBack(&owner);

    CHECK(flight.Remaining() == Milliseconds{3000});
    CHECK(flight.Velocity().x < 0.0F);
    REQUIRE(flight.Step(Milliseconds{1000}, nothing) == FlightState::Flying);
    CHECK(flight.Position().x == doctest::Approx(0.0F).epsilon(0.01));
    CHECK(flight.Position().z == doctest::Approx(AimPoint(owner).z).epsilon(0.001));
    CHECK(turnedAt.x == doctest::Approx(180.0F));
}

}
}
