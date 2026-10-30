#pragma once

#include <doctest/doctest.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include "app/frame_loop.hpp"
#include "app/race_step.hpp"
#include "app/scene.hpp"
#include "core/duration.hpp"
#include "sim/input_frame.hpp"
#include "sim/physics/racer_controls.hpp"
#include "sim/race/race_session.hpp"
#include "sim/replay/replay_recorder.hpp"
#include "sim/replay/snapshot_hash.hpp"

namespace rr::test {

/// A race with the player in slot 0, without drawing anything.
class RaceDriveScene final : public app::Scene {
public:
    RaceDriveScene(const sim::RaceSetup& setup, const sim::RaceCourse& course)
        : session_(Start(setup, course)), controls_(session_.Racers().size()) {}

    void Tick(const sim::InputFrame& input) override {
        app::StepRace(session_, input, controls_);
        for (std::size_t i = 0; i < controls_.size(); ++i) {
            respawns_ += session_.WasRespawned(i) ? 1 : 0;
            mostLaps_ = std::max(mostLaps_, session_.Progress().OfRacer(i).laps.CompletedLaps());
        }
    }

    void Snapshot(sim::RenderSnapshot& out) const override { app::SnapshotRace(session_, out); }

    void Render(const sim::RenderSnapshot& /*interpolated*/) override {}

    [[nodiscard]] int Respawns() const { return respawns_; }
    [[nodiscard]] int MostLaps() const { return mostLaps_; }

private:
    static sim::RaceSession Start(const sim::RaceSetup& setup, const sim::RaceCourse& course) {
        Result<sim::RaceSession> session = sim::RaceSession::Create(setup, course);
        REQUIRE(session.has_value());
        return std::move(*session);
    }

    sim::RaceSession session_;
    std::vector<sim::RacerControls> controls_;
    int respawns_ = 0;
    int mostLaps_ = 0;
};

/// Feeds a replay's input to the scene and keeps every tick's snapshot hash.
class ReplayDriver final : public app::TickTap {
public:
    explicit ReplayDriver(sim::ReplayPlayer& player) : player_(&player) {}

    void SceneStarted() override {}

    void BeforeTick(sim::InputFrame& input) override {
        isReplaying_ = !player_->IsFinished();
        if (isReplaying_) {
            input = player_->Next();
        }
    }

    void AfterTick(const sim::InputFrame& /*input*/, const sim::RenderSnapshot& after) override {
        if (isReplaying_) {
            hashes.push_back(sim::HashSnapshot(after));
            player_->Check(after);
        }
    }

    std::vector<std::uint64_t> hashes;

private:
    sim::ReplayPlayer* player_;
    bool isReplaying_ = false;
};

/// The scene that ran, for checking what the race went through.
struct DriveResult {
    std::vector<std::uint64_t> hashes;
    std::size_t checkedTicks = 0;
    std::optional<std::uint64_t> firstMismatch;
    int respawns = 0;
    int mostLaps = 0;
};

/// Plays `replay` through a frame loop that renders at `frameTimes`.
[[nodiscard]] inline DriveResult Drive(const sim::Replay& replay,
                                       std::vector<std::uint64_t> expected,
                                       const sim::RaceSetup& setup, const sim::RaceCourse& course,
                                       std::span<const Nanoseconds> frameTimes) {
    sim::ReplayPlayer player(replay, std::move(expected));
    ReplayDriver driver(player);
    app::FrameLoop loop;
    loop.SetTickTap(&driver);
    auto owned = std::make_unique<RaceDriveScene>(setup, course);
    const RaceDriveScene& scene = *owned;
    loop.Scenes().Push(std::move(owned));
    for (const Nanoseconds now : frameTimes) {
        static_cast<void>(loop.RunFrame(now));
    }
    return {.hashes = std::move(driver.hashes),
            .checkedTicks = player.CheckedTicks(),
            .firstMismatch = player.FirstMismatch(),
            .respawns = scene.Respawns(),
            .mostLaps = scene.MostLaps()};
}

}
