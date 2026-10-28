#include <doctest/doctest.h>

#include <chrono>
#include <cstdint>
#include <memory>
#include <vector>

#include "app/frame_loop.hpp"
#include "frame_times.hpp"
#include "sim/physics/plane_world.hpp"
#include "sim/physics/racer.hpp"
#include "sim/physics/racer_controls.hpp"
#include "sim/physics/surface.hpp"

namespace rr::app {
namespace {

struct TickState {
    Transform pose;
    Vector3 velocity;

    friend bool operator==(const TickState&, const TickState&) = default;
};

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

class RacerScene final : public Scene {
public:
    explicit RacerScene(std::vector<TickState>& states) : states_(states) {}

    void Tick(const sim::InputFrame& /*input*/) override {
        racer_.Tick(kSimTick, ScriptedControls(tick_++), world_);
        states_.push_back({.pose = racer_.Pose(), .velocity = racer_.Body().motion.velocity});
    }

    void Snapshot(sim::RenderSnapshot& out) const override {
        out.entities.assign(1, {.id = sim::EntityId{1}, .transform = racer_.Pose()});
    }

    void Render(const sim::RenderSnapshot& /*interpolated*/) override {}

private:
    std::vector<TickState>& states_;
    test::PlaneWorld world_ = CourseWorld();
    sim::Racer racer_{{}, {}, {.position = {.z = 0.2F}}};
    std::uint64_t tick_ = 0;
};

std::vector<TickState> Run(const std::vector<Nanoseconds>& frameTimes) {
    std::vector<TickState> states;
    FrameLoop loop;
    loop.Scenes().Push(std::make_unique<RacerScene>(states));
    for (const Nanoseconds now : frameTimes) {
        static_cast<void>(loop.RunFrame(now));
    }
    return states;
}

TEST_CASE("a scripted drive gives identical per-tick racer state at every framerate") {
    const Nanoseconds duration = std::chrono::seconds{7};
    const std::vector<TickState> uncapped = Run(test::UncappedFrames(duration));

    REQUIRE(uncapped.size() == duration / kSimTick);
    for (const int fps : {30, 60, 144}) {
        CAPTURE(fps);
        CHECK(Run(test::FixedRateFrames(fps, duration)) == uncapped);
    }
}

}
}
