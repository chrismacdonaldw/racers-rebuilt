#include "app/powerup/cursed_cars.hpp"

#include <doctest/doctest.h>

#include <cstdint>
#include <optional>
#include <utility>
#include <vector>

#include "../../sim/physics/plane_world.hpp"
#include "../../sim/race/checkpoint_ring.hpp"
#include "app/race_step.hpp"
#include "app/track_space.hpp"
#include "sim/physics/racer_controls.hpp"
#include "sim/powerup/yellow_effects.hpp"
#include "sim/race/race_session.hpp"

namespace rr::app {
namespace {

constexpr float kFrameMs = 16.0F;
constexpr Milliseconds kTick{12};
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

sim::RenderSnapshot TwoCars() {
    sim::RenderSnapshot snapshot = OneCar();
    snapshot.entities.push_back({.transform = TransformToEngine({.position = {}})});
    snapshot.racers.push_back({});
    return snapshot;
}

std::optional<Vector3> CurseTrapAt(const sim::RenderSnapshot& snapshot) {
    for (const sim::EffectSnapshot& effect : snapshot.effects) {
        if (effect.kind == sim::PowerupKind::Curse) {
            return TransformToGame(effect.transform).position;
        }
    }
    return std::nullopt;
}

/// The player drops curses beside a parked computer racer.
class CurseRace {
public:
    CurseRace() : session_(Start()) {
        while (session_.Clock().Phase() != sim::RacePhase::Racing) {
            Tick();
        }
    }

    void Use(assets::BrickColour colour, std::uint8_t level) {
        session_.GivePowerup(0, colour, level);
        controls_[0].isUsePressed = true;
        Tick();
        controls_[0].isUsePressed = false;
    }

    void Tick() { session_.Tick(kTick, controls_); }

    [[nodiscard]] const sim::RaceSession& Session() const { return session_; }

private:
    sim::RaceSession Start() {
        sim::RaceSetup setup{.racers = std::vector<sim::RacerEntry>(grid_.size())};
        setup.racers[0].isPlayer = true;
        sim::AddYellowEffects(setup.effects);
        Result<sim::RaceSession> session = sim::RaceSession::Create(
            setup, {.checkpoints = &graph_, .grid = grid_, .world = &world_, .lowestTrackZ = -100});
        REQUIRE(session.has_value());
        return std::move(*session);
    }

    sim::CheckpointGraph graph_ = test::Ring();
    test::PlaneWorld world_{{test::TestPlane{}}};
    std::vector<sim::RacerStart> grid_{{.position = {.x = -20.0F, .z = 0.2F}},
                                       {.position = {.x = -20.0F, .y = 8.0F, .z = 0.2F}}};
    sim::RaceSession session_;
    std::vector<sim::RacerControls> controls_ = std::vector<sim::RacerControls>(2);
};

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

TEST_CASE("a car cursed by a trap takes the curse from that trap") {
    CursedCars cars;
    sim::RenderSnapshot snapshot = OneCar();
    const Vector3 trap = kCar + Vector3{.x = 50.0F, .z = 13.0F};
    snapshot.racers[0].curseSource.trap = ToEngine(trap);

    cars.Update(snapshot, 0.0F);

    REQUIRE(cars.Orbits().size() == 1);
    CHECK(CurseAt(cars, 0) == trap);
}

TEST_CASE("a curse passed between cars flies from the car that lost it") {
    CursedCars cars;
    sim::RenderSnapshot snapshot = TwoCars();
    cars.Update(snapshot, kFrameMs);
    const std::optional<Vector3> held = CurseAt(cars, 0);
    REQUIRE(held.has_value());

    snapshot.racers[0].isCursed = false;
    snapshot.racers[1].isCursed = true;
    snapshot.racers[1].curseSource.giver = 0;
    cars.Update(snapshot, 0.0F);

    CHECK_FALSE(CurseAt(cars, 0).has_value());
    CHECK(CurseAt(cars, 1) == held);
}

TEST_CASE("a curse that runs out doesn't fly to a car a trap caught in the same frame") {
    CursedCars cars;
    sim::RenderSnapshot snapshot = TwoCars();
    cars.Update(snapshot, kFrameMs);
    const Vector3 trap{.x = -40.0F, .z = 13.0F};

    snapshot.racers[0].isCursed = false;
    snapshot.racers[1].isCursed = true;
    snapshot.racers[1].curseSource.trap = ToEngine(trap);
    cars.Update(snapshot, 0.0F);

    CHECK(CurseAt(cars, 1) == trap);
}

TEST_CASE("a car a dropped curse catches in a race starts its curse at that curse") {
    CurseRace race;
    race.Use(assets::BrickColour::Yellow, 3);
    for (int tick = 0; tick < 50 && !race.Session().Racers()[1].Status().isCursed; ++tick) {
        race.Tick();
    }
    REQUIRE(race.Session().Racers()[1].Status().isCursed);
    sim::RenderSnapshot snapshot;
    SnapshotRace(race.Session(), snapshot);
    const std::optional<Vector3> trap = CurseTrapAt(snapshot);
    REQUIRE(trap.has_value());

    CursedCars cars;
    cars.Update(snapshot, 0.0F);

    CHECK(CurseAt(cars, 1) == trap);
}

}
}
