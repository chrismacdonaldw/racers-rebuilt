#include <doctest/doctest.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <vector>

#include "app/frame_loop.hpp"
#include "frame_times.hpp"
#include "sim/physics/plane_world.hpp"
#include "sim/physics/racer.hpp"
#include "sim/physics/racer_chassis.hpp"
#include "sim/physics/racer_controls.hpp"
#include "sim/physics/surface.hpp"
#include "sim/replay/snapshot_hash.hpp"

namespace rr::app {
namespace {

sim::RacerControls ScriptedControls(std::uint64_t tick) {
    const std::uint64_t ms = tick * static_cast<std::uint64_t>(kSimTick.count());
    if (ms < 1500) {
        return {.isThrottleHeld = true};
    }
    if (ms < 3000) {
        return {.isLeftHeld = true, .isThrottleHeld = true};
    }
    if (ms < 4000) {
        return {.isLeftHeld = true, .isThrottleHeld = true, .isDriftHeld = true};
    }
    if (ms < 5500) {
        return {.isRightHeld = true, .isBrakeHeld = true};
    }
    return {.isThrottleHeld = true};
}

test::PlaneWorld CourseWorld() {
    return test::PlaneWorld({
        {.maxX = 80.0F},
        {.surface = {.rollingResistance = 20.0F}, .minX = 80.0F},
        {.normal = {.y = -1.0F}, .offset = -90.0F},
    });
}

struct Car {
    sim::ChassisSpec chassis;
    sim::CarBuild build;
};

Car BuiltCar() {
    const assets::ChassisEntry chassis{.builtCentreOfMass = {.x = -0.4F, .z = 1.2F},
                                       .baseMass = 2400.0F,
                                       .handling = 60,
                                       .topSpeed = 70,
                                       .acceleration = 90};
    return {.chassis = sim::BuiltChassis(chassis),
            .build = sim::BuiltCarBuild(
                {.cells = 214, .centroid = {.x = -0.8178F, .y = -0.0935F, .z = 6.4486F}})};
}

class RacerScene final : public Scene {
public:
    explicit RacerScene(const Car& car)
        : racer_(car.chassis, car.build, {.position = {.z = 0.2F}}) {}

    void Tick(const sim::InputFrame& /*input*/) override {
        racer_.Tick(kSimTick, ScriptedControls(tick_++), world_);
    }

    void Snapshot(sim::RenderSnapshot& out) const override {
        out.entities.assign(1, {.id = sim::EntityId{1}, .transform = racer_.Pose()});
        out.simStateHash = sim::HashRacerState(racer_);
    }

    void Render(const sim::RenderSnapshot& /*interpolated*/) override {}

private:
    test::PlaneWorld world_ = CourseWorld();
    sim::Racer racer_;
    std::uint64_t tick_ = 0;
};

class HashTap final : public TickTap {
public:
    void SceneStarted() override { hashes.clear(); }
    void BeforeTick(sim::InputFrame& /*input*/) override {}
    void AfterTick(const sim::InputFrame& /*input*/, const sim::RenderSnapshot& after) override {
        hashes.push_back(sim::HashSnapshot(after));
    }

    std::vector<std::uint64_t> hashes;
};

std::vector<std::uint64_t> Run(const std::vector<Nanoseconds>& frameTimes, const Car& car = {}) {
    HashTap tap;
    FrameLoop loop;
    loop.SetTickTap(&tap);
    loop.Scenes().Push(std::make_unique<RacerScene>(car));
    for (const Nanoseconds now : frameTimes) {
        static_cast<void>(loop.RunFrame(now));
    }
    return tap.hashes;
}

TEST_CASE("a scripted drive gives identical per-tick snapshot hashes at every framerate") {
    const Nanoseconds duration = std::chrono::seconds{7};
    const std::vector<std::uint64_t> uncapped = Run(test::UncappedFrames(duration));

    REQUIRE(uncapped.size() == duration / kSimTick);
    for (const int fps : {30, 60, 144}) {
        CAPTURE(fps);
        CHECK(Run(test::FixedRateFrames(fps, duration)) == uncapped);
    }
}

TEST_CASE("a built car's drive gives identical per-tick snapshot hashes at every framerate") {
    const Nanoseconds duration = std::chrono::seconds{7};
    const std::vector<std::uint64_t> uncapped = Run(test::UncappedFrames(duration), BuiltCar());

    REQUIRE(uncapped.size() == duration / kSimTick);
    CHECK(uncapped != Run(test::UncappedFrames(duration)));
    for (const int fps : {30, 60, 144}) {
        CAPTURE(fps);
        CHECK(Run(test::FixedRateFrames(fps, duration), BuiltCar()) == uncapped);
    }
}

}
}
