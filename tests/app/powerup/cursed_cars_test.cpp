#include "app/powerup/cursed_cars.hpp"

#include <doctest/doctest.h>

#include <optional>

#include "app/track_space.hpp"

namespace rr::app {
namespace {

constexpr float kFrameMs = 16.0F;
constexpr Vector3 kCar{.x = 100.0F, .y = 50.0F};

sim::RenderSnapshot OneCar() {
    sim::RenderSnapshot snapshot;
    snapshot.entities.push_back({.transform = TransformToEngine({.position = kCar})});
    snapshot.racers.push_back({.isCursed = true});
    return snapshot;
}

std::optional<Vector3> CurseAt(const CursedCars& cars, std::size_t racer) {
    const std::optional<CurseOrbit>& orbit = cars.Orbits()[racer];
    return orbit ? std::optional(orbit->Pose().position) : std::nullopt;
}

TEST_CASE("the curse circles above the car once a second, facing it") {
    CurseOrbit orbit(kCar + Vector3{.y = 6.0F, .z = 9.0F});

    orbit.Follow(kCar, 0.0F, kFrameMs);
    orbit.Follow(kCar, 0.0F, 250.0F);

    const Vector3 at = orbit.Pose().position;
    CHECK(at.x == doctest::Approx(kCar.x - 6.0F));
    CHECK(at.y == doctest::Approx(kCar.y));
    CHECK(at.z == doctest::Approx(9.0F));
    const Vector3 side = Rotate(orbit.Pose().rotation, Vector3{.x = 1.0F});
    CHECK(side.y == doctest::Approx(1.0F));
}

TEST_CASE("the curse chases its point no faster than the car, and at least 0.1 per ms") {
    const Vector3 start = kCar + Vector3{.x = 100.0F};
    CurseOrbit slow(start);
    CurseOrbit fast(start);

    slow.Follow(kCar, 0.0F, 10.0F);
    fast.Follow(kCar, -2.0F, 10.0F);

    CHECK(Length(slow.Pose().position - start) == doctest::Approx(1.0F));
    CHECK(Length(fast.Pose().position - start) == doctest::Approx(20.0F));
}

TEST_CASE("the curse shrinks to two thirds") {
    CurseOrbit orbit(kCar);

    orbit.Follow(kCar, 0.0F, 100.0F);
    CHECK(orbit.Scale() == doctest::Approx(1.0F - 0.066F));
    orbit.Follow(kCar, 0.0F, 1000.0F);
    CHECK(orbit.Scale() == doctest::Approx(0.666F));
}

TEST_CASE("a newly cursed car takes the curse from the nearest curse power-up") {
    CursedCars cars;
    sim::RenderSnapshot snapshot = OneCar();
    const Vector3 trap = kCar + Vector3{.x = 50.0F, .z = 13.0F};
    snapshot.effects.push_back(
        {.kind = sim::PowerupKind::Curse, .transform = TransformToEngine({.position = trap})});

    cars.Update(snapshot, 0.0F);

    REQUIRE(cars.Orbits().size() == 1);
    CHECK(CurseAt(cars, 0) == trap);
}

TEST_CASE("a curse passed between cars flies from the car that lost it") {
    CursedCars cars;
    sim::RenderSnapshot snapshot = OneCar();
    snapshot.entities.push_back({.transform = TransformToEngine({.position = {}})});
    snapshot.racers.push_back({});
    cars.Update(snapshot, kFrameMs);
    const std::optional<Vector3> held = CurseAt(cars, 0);
    REQUIRE(held.has_value());

    snapshot.racers[0].isCursed = false;
    snapshot.racers[1].isCursed = true;
    cars.Update(snapshot, 0.0F);

    CHECK_FALSE(CurseAt(cars, 0).has_value());
    CHECK(CurseAt(cars, 1) == held);
}

}
}
