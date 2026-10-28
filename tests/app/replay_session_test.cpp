#include "app/replay_session.hpp"

#include <doctest/doctest.h>

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "app/frame_loop.hpp"
#include "app/orbiting_cube.hpp"
#include "frame_times.hpp"
#include "temp_folder.hpp"

namespace rr::app {
namespace {

constexpr Nanoseconds kSession = std::chrono::seconds{10};

class CubeScene final : public Scene {
public:
    explicit CubeScene(bool isAltered = false) : isAltered_(isAltered) {}

    void Tick(const sim::InputFrame& input) override { cube_.Tick(input); }

    void Snapshot(sim::RenderSnapshot& out) const override {
        Transform pose = cube_.Pose();
        pose.position.y += isAltered_ ? 0.001F : 0.0F;
        out.entities.assign(1, {.id = sim::EntityId{1}, .transform = pose});
    }

    void Render([[maybe_unused]] const sim::RenderSnapshot& interpolated) override {}

private:
    OrbitingCube cube_;
    bool isAltered_;
};

struct KeyChange {
    Nanoseconds at;
    sim::Action action;
    bool isDown;
};

constexpr std::array kScript{
    KeyChange{
        .at = std::chrono::milliseconds{900}, .action = sim::Action::Accelerate, .isDown = true},
    KeyChange{
        .at = std::chrono::milliseconds{2400}, .action = sim::Action::SteerLeft, .isDown = true},
    KeyChange{
        .at = std::chrono::milliseconds{3100}, .action = sim::Action::SteerLeft, .isDown = false},
    KeyChange{
        .at = std::chrono::milliseconds{5000}, .action = sim::Action::Accelerate, .isDown = false},
    KeyChange{.at = std::chrono::milliseconds{5003}, .action = sim::Action::Brake, .isDown = true},
    KeyChange{
        .at = std::chrono::milliseconds{7700}, .action = sim::Action::SteerRight, .isDown = true},
    KeyChange{.at = std::chrono::milliseconds{9100}, .action = sim::Action::Brake, .isDown = false},
};

void Run(TickTap& tap, std::unique_ptr<Scene> scene, const std::vector<Nanoseconds>& frames,
         std::span<const KeyChange> script) {
    FrameLoop loop;
    loop.SetTickTap(&tap);
    loop.Scenes().Push(std::move(scene));
    auto next = script.begin();
    for (const Nanoseconds now : frames) {
        for (; next != script.end() && next->at <= now; ++next) {
            if (next->isDown) {
                loop.Input().Press(next->action);
            } else {
                loop.Input().Release(next->action);
            }
        }
        static_cast<void>(loop.RunFrame(now));
    }
}

struct Outcome {
    bool isFinished = false;
    std::size_t checkedTicks = 0;
    std::optional<std::uint64_t> firstMismatch;

    friend bool operator==(const Outcome&, const Outcome&) = default;
};

class RecordedSession {
public:
    RecordedSession() {
        ReplayRecording recording(file_, "", 7);
        Run(recording, std::make_unique<CubeScene>(), test::UncappedFrames(kSession), kScript);
        isSaved_ = recording.Save().has_value();
    }

    [[nodiscard]] Outcome Play(const std::vector<Nanoseconds>& frames,
                               bool isAltered = false) const {
        Result<ReplayPlayback> playback = ReplayPlayback::Load(file_);
        if (!isSaved_ || !playback) {
            return {};
        }
        Run(*playback, std::make_unique<CubeScene>(isAltered), frames, {});
        const sim::ReplayPlayer& player = playback->Player();
        return {.isFinished = player.IsFinished(),
                .checkedTicks = player.CheckedTicks(),
                .firstMismatch = player.FirstMismatch()};
    }

private:
    test::TempFolder folder_;
    std::filesystem::path file_ = folder_.Path() / "session.rrr";
    bool isSaved_ = false;
};

TEST_CASE("a recorded session replays to the same snapshots at any framerate") {
    const RecordedSession session;
    const std::size_t ticks = kSession / kSimTick;

    for (const int fps : {30, 60, 144, 0}) {
        CAPTURE(fps);
        const std::vector<Nanoseconds> frames =
            fps == 0 ? test::UncappedFrames(kSession) : test::FixedRateFrames(fps, kSession);
        CHECK(session.Play(frames) == Outcome{.isFinished = true, .checkedTicks = ticks});
    }
}

TEST_CASE("playback finds the first tick that drifts from the recording") {
    const RecordedSession session;

    const Outcome outcome = session.Play(test::FixedRateFrames(60, kSession), true);

    CHECK(outcome.firstMismatch == 1);
}

}
}
