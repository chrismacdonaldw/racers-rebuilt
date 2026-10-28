#include "app/frame_loop.hpp"

#include <doctest/doctest.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <format>
#include <memory>
#include <string>
#include <vector>

#include "frame_times.hpp"

namespace rr::app {
namespace {

using std::chrono::milliseconds;
using std::chrono::seconds;

class PaceScene final : public Scene {
public:
    void Tick(const sim::InputFrame& input) override {
        velocity_ += kAcceleration;
        position_ += velocity_;
        positions.push_back(position_);
        inputs.push_back(input);
    }

    void Snapshot(sim::RenderSnapshot& out) const override {
        out.entities.assign(1,
                            {.id = sim::EntityId{1}, .transform = {.position = {.x = position_}}});
    }

    void Render(const sim::RenderSnapshot& interpolated) override {
        rendered.push_back(interpolated.entities.front().transform.position.x);
        cameraHeight = interpolated.camera.transform.position.y;
    }

    std::vector<float> positions;
    std::vector<sim::InputFrame> inputs;
    std::vector<float> rendered;
    float cameraHeight = 0.0F;

private:
    static constexpr float kAcceleration = 0.01F;

    float position_ = 0.0F;
    float velocity_ = 0.0F;
};

struct LoopUnderTest {
    LoopUnderTest() {
        auto owned = std::make_unique<PaceScene>();
        scene = owned.get();
        loop.Scenes().Push(std::move(owned));
    }

    FrameLoop loop;
    PaceScene* scene = nullptr;
};

struct RunResult {
    std::vector<float> positions;
    std::vector<FrameStats> frames;
};

RunResult Run(const std::vector<Nanoseconds>& frameTimes) {
    LoopUnderTest test;
    RunResult result;
    for (const Nanoseconds now : frameTimes) {
        result.frames.push_back(test.loop.RunFrame(now));
    }
    result.positions = test.scene->positions;
    return result;
}

TEST_CASE("every framerate produces the same ticks and per-tick state") {
    const Nanoseconds duration = seconds{10};
    const RunResult uncapped = Run(test::UncappedFrames(duration));

    CHECK(uncapped.positions.size() == duration / kSimTick);
    for (const int fps : {30, 60, 144}) {
        CAPTURE(fps);
        CHECK(Run(test::FixedRateFrames(fps, duration)).positions == uncapped.positions);
    }
}

TEST_CASE("alpha stays within one tick") {
    for (const FrameStats& frame : Run(test::UncappedFrames(seconds{2})).frames) {
        CHECK(frame.alpha >= 0.0F);
        CHECK(frame.alpha < 1.0F);
    }
}

TEST_CASE("rendering falls between the last two ticks") {
    LoopUnderTest test;
    static_cast<void>(test.loop.RunFrame(Nanoseconds{0}));
    static_cast<void>(test.loop.RunFrame(milliseconds{30}));

    const std::vector<float>& positions = test.scene->positions;
    REQUIRE(positions.size() == 2);
    CHECK(test.scene->rendered.back() > positions[0]);
    CHECK(test.scene->rendered.back() < positions[1]);
}

TEST_CASE("a 2 s stall runs at most kMaxStepsPerFrame ticks and drops the rest") {
    LoopUnderTest test;
    static_cast<void>(test.loop.RunFrame(Nanoseconds{0}));

    const FrameStats stall = test.loop.RunFrame(seconds{2});
    const FrameStats next = test.loop.RunFrame(seconds{2} + milliseconds{1});

    CHECK(stall.tickCount == kMaxStepsPerFrame);
    CHECK(next.tickCount == 0);
}

TEST_CASE("a tap shorter than one tick reaches the next tick's input") {
    LoopUnderTest test;
    static_cast<void>(test.loop.RunFrame(Nanoseconds{0}));
    test.loop.Input().Press(sim::Action::UsePowerup);
    test.loop.Input().Release(sim::Action::UsePowerup);
    static_cast<void>(test.loop.RunFrame(milliseconds{5}));

    static_cast<void>(test.loop.RunFrame(milliseconds{13}));

    REQUIRE(test.scene->inputs.size() == 1);
    CHECK(test.scene->inputs[0].WasPressed(sim::Action::UsePowerup));
}

TEST_CASE("an empty scene stack runs no ticks") {
    FrameLoop loop;

    static_cast<void>(loop.RunFrame(Nanoseconds{0}));

    CHECK(loop.RunFrame(seconds{1}).tickCount == 0);
}

class HookLog {
public:
    explicit HookLog(mods::HookRegistry& hooks) {
        hooks.Subscribe<mods::SceneEntered>([this](auto&) { events.emplace_back("entered"); });
        hooks.Subscribe<mods::SceneExited>([this](auto&) { events.emplace_back("exited"); });
        hooks.Subscribe<mods::TickStarted>([this](const mods::TickStarted& tick) {
            events.push_back(std::format("start {}", tick.tick));
        });
        hooks.Subscribe<mods::TickEnded>([this](const mods::TickEnded& tick) {
            events.push_back(std::format("end {}", tick.tick));
        });
        hooks.Subscribe<mods::FrameRendering>([this](mods::FrameRendering& frame) {
            events.emplace_back("render");
            frame.camera.position.y = kCameraHeight;
        });
    }

    static constexpr float kCameraHeight = 7.0F;
    std::vector<std::string> events;
};

TEST_CASE("hooks fire around each tick and before each render") {
    LoopUnderTest test;
    const HookLog log(test.loop.Hooks());

    static_cast<void>(test.loop.RunFrame(Nanoseconds{0}));
    static_cast<void>(test.loop.RunFrame(milliseconds{30}));

    CHECK(log.events == std::vector<std::string>{"entered", "render", "start 1", "end 1", "start 2",
                                                 "end 2", "render"});
    CHECK(test.scene->cameraHeight == HookLog::kCameraHeight);
}

TEST_CASE("replacing the scene fires exited, then entered") {
    LoopUnderTest test;
    static_cast<void>(test.loop.RunFrame(Nanoseconds{0}));
    const HookLog log(test.loop.Hooks());

    test.loop.Scenes().Pop();
    test.loop.Scenes().Push(std::make_unique<PaceScene>());
    static_cast<void>(test.loop.RunFrame(milliseconds{1}));

    CHECK(log.events == std::vector<std::string>{"exited", "entered", "render"});
}

}
}
